#include "musicat/stream_codec.h"
#include "musicat/audio_config.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>

#include <libavutil/avassert.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libswresample/swresample.h>
}

namespace musicat::stream_codec
{

int
stream_codec_t::write_frame (AVCodecContext *c, AVFrame *frame)
{
    int ret;

    // send the frame to the encoder
    ret = avcodec_send_frame (c, frame);
    if (ret < 0)
        {
            fprintf (stderr, "Error sending a frame to the encoder: %s\n", av_err2str (ret));
            return ret;
        }

    return 0;
}

int
stream_codec_t::add_stream (AVFormatContext *oc, const AVCodec **codec, enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int ret;

    /* find the encoder */
    *codec = avcodec_find_encoder (codec_id);
    if (!(*codec))
        {
            fprintf (stderr, "Could not find encoder for '%s'\n", avcodec_get_name (codec_id));
            return -1;
        }

    ost.tmp_pkt = av_packet_alloc ();
    if (!ost.tmp_pkt)
        {
            fprintf (stderr, "Could not allocate AVPacket\n");
            return -1;
        }

    ost.st = avformat_new_stream (oc, NULL);
    if (!ost.st)
        {
            fprintf (stderr, "Could not allocate stream\n");
            return -1;
        }
    ost.st->id = oc->nb_streams - 1;
    c = avcodec_alloc_context3 (*codec);
    if (!c)
        {
            fprintf (stderr, "Could not alloc an encoding context\n");
            return -1;
        }
    ost.enc = c;

    const void *codec_config;
    c->bit_rate = 128000;
    ret = avcodec_get_supported_config (c, NULL, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &codec_config, NULL);
    if (ret < 0)
        {
            fprintf (stderr, "Failed to get supported sample formats\n");
            return ret;
        }
    c->sample_fmt = codec_config ? *(const enum AVSampleFormat *)codec_config : AV_SAMPLE_FMT_FLTP;
    ret = avcodec_get_supported_config (c, NULL, AV_CODEC_CONFIG_SAMPLE_RATE, 0, &codec_config, NULL);
    if (ret < 0)
        {
            fprintf (stderr, "Failed to get supported sample rates\n");
            return ret;
        }
    if (codec_config)
        {
            const int *supported_samplerates = (const int *)codec_config;
            c->sample_rate = supported_samplerates[0];
            for (; *supported_samplerates; supported_samplerates++)
                {
                    if (*supported_samplerates == 48000)
                        c->sample_rate = 48000;
                }
        }
    else
        {
            c->sample_rate = 48000;
        }
    AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy (&c->ch_layout, &ch_layout);
    ost.st->time_base = (AVRational){ 1, c->sample_rate };

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    return 0;
}

AVFrame *
stream_codec_t::alloc_audio_frame (enum AVSampleFormat sample_fmt, const AVChannelLayout *channel_layout, int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc ();
    if (!frame)
        {
            fprintf (stderr, "Error allocating an audio frame\n");
            return NULL;
        }

    frame->format = sample_fmt;
    av_channel_layout_copy (&frame->ch_layout, channel_layout);
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples)
        {
            if (av_frame_get_buffer (frame, 0) < 0)
                {
                    fprintf (stderr, "Error allocating an audio buffer\n");
                    return NULL;
                }
        }

    return frame;
}

int
stream_codec_t::open_audio (AVFormatContext *oc, const AVCodec *codec, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost.enc;

    /* open it */
    av_dict_copy (&opt, opt_arg, 0);
    ret = avcodec_open2 (c, codec, &opt);
    av_dict_free (&opt);
    if (ret < 0)
        {
            fprintf (stderr, "Could not open audio codec: %s\n", av_err2str (ret));
            return ret;
        }

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;

    ost.frame = alloc_audio_frame (c->sample_fmt, &c->ch_layout, c->sample_rate, nb_samples);
    if (!ost.frame)
        return -1;

    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context (ost.st->codecpar, c);
    if (ret < 0)
        {
            fprintf (stderr, "Could not copy the stream parameters\n");
            return ret;
        }

    /* create resampler context */
    ost.swr_ctx = swr_alloc ();
    if (!ost.swr_ctx)
        {
            fprintf (stderr, "Could not allocate resampler context\n");
            return ret;
        }

    /* set options */
    av_opt_set_chlayout (ost.swr_ctx, "in_chlayout", &c->ch_layout, 0);
    av_opt_set_int (ost.swr_ctx, "in_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt (ost.swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_chlayout (ost.swr_ctx, "out_chlayout", &c->ch_layout, 0);
    av_opt_set_int (ost.swr_ctx, "out_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt (ost.swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init (ost.swr_ctx)) < 0)
        {
            fprintf (stderr, "Failed to initialize the resampling context\n");
            return ret;
        }

    return 0;
}

int
stream_codec_t::setup_io_context (void *userdata, int (*ogg_page_callback) (void *opaque, const uint8_t *buf, int buf_size))
{
    AVIOContext *avio_ctx;
    int ret;

    // custom IO context
    if (oc->pb)
        fprintf (stderr, "WARNING: Original avio_ctx exists!!!\n");

    uint8_t *buffer = NULL, *avio_ctx_buffer = NULL;
    size_t buffer_size, avio_ctx_buffer_size = 4096;

    avio_ctx_buffer = (uint8_t *)av_malloc (avio_ctx_buffer_size);
    if (!avio_ctx_buffer)
        {
            ret = AVERROR (ENOMEM);
            av_log (oc, AV_LOG_FATAL, "Error initializing output context buffer: %s\n", av_err2str (ret));
            return ret;
        }
    avio_ctx = avio_alloc_context (avio_ctx_buffer, avio_ctx_buffer_size, 1, userdata, NULL, ogg_page_callback, NULL);
    if (!avio_ctx)
        {
            av_freep (&avio_ctx_buffer);
            ret = AVERROR (ENOMEM);
            return ret;
        }
    oc->pb = avio_ctx;

    return 0;
}

void
stream_codec_t::close_stream ()
{
    avcodec_free_context (&ost.enc);
    av_frame_free (&ost.frame);
    av_packet_free (&ost.tmp_pkt);
    swr_free (&ost.swr_ctx);
}

////////////////////////////////////////////////////////////////////////////////

int
stream_codec_t::init (void *userdata, int (*ogg_page_callback) (void *opaque, const uint8_t *buf, int buf_size))
{
    const AVCodec *audio_codec;
    int ret;

    avformat_alloc_output_context2 (&oc, NULL, "opus", NULL);
    if (!oc)
        return 1;

    fmt = oc->oformat;

    if (fmt->audio_codec != AV_CODEC_ID_NONE)
        {
            ret = add_stream (oc, &audio_codec, fmt->audio_codec);
            if (ret)
                return ret;
        }

    // set encoder options
    if (av_dict_set (&opt, "application", "audio", 0) < 0)
        fprintf (stderr, "Failed to set application type\n");
    if (av_dict_set (&opt, "frame_duration", "60.0", 0) < 0)
        fprintf (stderr, "Failed to set frame duration\n");

    if ((ret = open_audio (oc, audio_codec, opt)))
        return ret;
    av_dump_format (oc, 0, NULL, 1);

    ret = setup_io_context (userdata, ogg_page_callback);
    if (ret)
        return ret;

    ended = false;
    return 0;
}

int
stream_codec_t::get_packet (AVPacket **out)
{
    int ret;

    AVCodecContext *c = ost.enc;
    AVStream *st = ost.st;
    AVPacket *pkt = ost.tmp_pkt;

    ret = avcodec_receive_packet (c, pkt);
    if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
        return ret;
    else if (ret < 0)
        {
            fprintf (stderr, "Error encoding a frame: %s\n", av_err2str (ret));
            return ret;
        }

    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts (pkt, c->time_base, st->time_base);
    pkt->stream_index = st->index;

    if (out)
        *out = pkt;

    return ret == AVERROR_EOF ? AVERROR_EOF : 0;
}

// returns 0 on success
int
stream_codec_t::write_pcm_frame (AVFrame *frame)
{
    AVCodecContext *c;
    int ret;
    int dst_nb_samples;

    if (!wrote_header)
        {
            /* Write the stream header, if any. */
            ret = avformat_write_header (oc, &opt);
            if (ret < 0)
                {
                    fprintf (stderr, "Error occurred when opening output file: %s\n", av_err2str (ret));
                    return 1;
                }
            wrote_header = true;
        }

    c = ost.enc;

    if (frame)
        {
            /* convert samples from native format to destination codec format, using
             * the resampler */
            /* compute destination number of samples */
            dst_nb_samples = swr_get_delay (ost.swr_ctx, c->sample_rate) + frame->nb_samples;
            av_assert0 (dst_nb_samples == frame->nb_samples);

            /* when we pass a frame to the encoder, it may keep a reference to it
             * internally;
             * make sure we do not overwrite it here
             */
            ret = av_frame_make_writable (ost.frame);
            if (ret < 0)
                return ret;

            /* convert to destination format */
            ret = swr_convert (ost.swr_ctx, ost.frame->data, dst_nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
            if (ret < 0)
                {
                    fprintf (stderr, "Error while converting\n");
                    return ret;
                }
            frame = ost.frame;

            frame->pts = av_rescale_q (ost.samples_count, (AVRational){ 1, c->sample_rate }, c->time_base);
            ost.samples_count += dst_nb_samples;
        }

    return write_frame (c, frame);
}

int
stream_codec_t::write_packet (AVPacket *pkt)
{
    int ret;
    /* Write the compressed frame to the media file. */
    ret = av_interleaved_write_frame (oc, pkt);
    /* pkt is now blank (av_interleaved_write_frame() takes ownership of
     * its contents and resets pkt), so that no unreferencing is necessary.
     * This would be different if one used av_write_frame(). */
    if (ret < 0)
        {
            fprintf (stderr, "Error while writing output packet: %s\n", av_err2str (ret));
            return ret;
        }
    return ret == AVERROR_EOF ? AVERROR_EOF : 0;
}

void
stream_codec_t::log_packet (const AVPacket *pkt)
{
    const AVFormatContext *fmt_ctx = oc;
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    fprintf (stderr,
             "pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s "
             "stream_index:%d\n",
             av_ts2str (pkt->pts), av_ts2timestr (pkt->pts, time_base), av_ts2str (pkt->dts), av_ts2timestr (pkt->dts, time_base),
             av_ts2str (pkt->duration), av_ts2timestr (pkt->duration, time_base), pkt->stream_index);
}

int
stream_codec_t::end ()
{
    if (ended)
        return -1;
    ended = true;

    av_write_trailer (oc);
    return 0;
}

int
stream_codec_t::destroy ()
{
    if (opt)
        {
            av_dict_free (&opt);
            opt = nullptr;
        }

    if (!oc)
        return -1;

    close_stream ();

    if (oc->pb)
        av_freep (&oc->pb->buffer);
    avio_context_free (&oc->pb);

    /* free the stream */
    avformat_free_context (oc);
    oc = nullptr;

    return 0;
}

} // musicat::stream_codec
