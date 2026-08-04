#ifndef MUSICAT_STREAM_CODEC_H
#define MUSICAT_STREAM_CODEC_H

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

namespace musicat::stream_codec
{

class stream_codec_t
{
    struct OutputStream
    {
        AVStream *st;
        AVCodecContext *enc;

        AVFrame *frame;
        AVPacket *tmp_pkt;

        struct SwrContext *swr_ctx;

        int samples_count;
    } ost;
    AVFormatContext *oc;
    const AVOutputFormat *fmt;

    int write_frame (AVCodecContext *c, AVFrame *frame);
    int add_stream (AVFormatContext *oc, const AVCodec **codec, enum AVCodecID codec_id);
    AVFrame *alloc_audio_frame (enum AVSampleFormat sample_fmt, const AVChannelLayout *channel_layout, int sample_rate, int nb_samples);
    int open_audio (AVFormatContext *oc, const AVCodec *codec, AVDictionary *opt_arg);
    int setup_io_context (void *userdata, int (*ogg_page_callback) (void *opaque, const uint8_t *buf, int buf_size));
    void close_stream ();

  public:
    AVDictionary *opt;
    bool wrote_header;
    bool ended;

    stream_codec_t () : ost ({ 0 }), oc (nullptr), fmt (nullptr), opt (nullptr), wrote_header (false), ended (true) {}
    ~stream_codec_t () { destroy (); }

    stream_codec_t (const stream_codec_t &) = delete;
    stream_codec_t &operator= (const stream_codec_t &) = delete;
    stream_codec_t (stream_codec_t &&) = delete;
    stream_codec_t &operator= (stream_codec_t &&) = delete;

    int init (void *userdata, int (*ogg_page_callback) (void *opaque, const uint8_t *buf, int buf_size));

    int get_packet (AVPacket **out = nullptr);
    // returns 0 on success
    int write_pcm_frame (AVFrame *frame);
    int write_packet (AVPacket *pkt);
    void log_packet (const AVPacket *pkt);

    int end ();
    int destroy ();
};

} // musicat::stream_codec

#endif // MUSICAT_STREAM_CODEC_H
