#include "musicat/decoder.h"
#include "musicat/config.h"

namespace musicat
{

int
decoder_t::open_input_file (const char *filename)
{
    const AVCodec *dec;
    int ret;

    if ((ret = avformat_open_input (&fmt_ctx, filename, NULL, NULL)) < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot open input file\n");
            return ret;
        }

    if ((ret = avformat_find_stream_info (fmt_ctx, NULL)) < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot find stream information\n");
            return ret;
        }

    /* select the audio stream */
    ret = av_find_best_stream (fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
    if (ret < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
            return ret;
        }
    audio_stream_index = ret;

    /* create decoding context */
    dec_ctx = avcodec_alloc_context3 (dec);
    if (!dec_ctx)
        return AVERROR (ENOMEM);
    avcodec_parameters_to_context (dec_ctx, fmt_ctx->streams[audio_stream_index]->codecpar);

    /* init the audio decoder */
    if ((ret = avcodec_open2 (dec_ctx, dec, NULL)) < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
            return ret;
        }

    /* set decoding to run on this thread only */
    dec_ctx->thread_count = 1;

    return 0;
}

int
decoder_t::read_frame ()
{
    if (got_eof)
        return 0;

    int ret;
    av_packet_unref (packet);
    if ((ret = av_read_frame (fmt_ctx, packet)) < 0)
        return ret;

    if (packet->stream_index == audio_stream_index)
        {
            ret = avcodec_send_packet (dec_ctx, packet);
            if (ret < 0)
                {
                    av_log (NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                }
        }

    return ret;
}

int
decoder_t::receive_frame ()
{
    if (got_eof)
        return 0;

    int ret;
    av_frame_unref (frame);
    do
        {
            ret = avcodec_receive_frame (dec_ctx, frame);
            if (ret == AVERROR_EOF)
                {
                    break;
                }

            if (ret != AVERROR (EAGAIN) && ret >= 0)
                {
                    /* push the audio data from decoded frame into the filtergraph */
                    if ((ret = av_buffersrc_add_frame_flags (buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF)) < 0)
                        {
                            av_log (NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
                        }
                    break;
                }

            if (ret == AVERROR (EAGAIN))
                {
                    ret = read_frame ();
                }

            if (ret == AVERROR_EOF)
                {
                    break;
                }

            if (ret < 0)
                {
                    av_log (NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                    break;
                }
        }
    while (1);

    return ret;
}

int
decoder_t::init_filters (const char *filters_descr)
{
    char args[512];
    int ret = 0;
    const AVFilter *abuffersrc = avfilter_get_by_name ("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name ("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc ();
    AVFilterInOut *inputs = avfilter_inout_alloc ();
    const AVFilterLink *outlink;
    AVRational time_base = fmt_ctx->streams[audio_stream_index]->time_base;

    filter_graph = avfilter_graph_alloc ();
    if (!outputs || !inputs || !filter_graph)
        {
            ret = AVERROR (ENOMEM);
            goto end;
        }

    /* set filter graph to run on this thread only */
    filter_graph->nb_threads = 1;

    /* buffer audio source: the decoded frames from the decoder will be inserted
     * here. */
    if (dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default (&dec_ctx->ch_layout, dec_ctx->ch_layout.nb_channels);
    ret = snprintf (args, sizeof (args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=", time_base.num, time_base.den,
                    dec_ctx->sample_rate, av_get_sample_fmt_name (dec_ctx->sample_fmt));
    av_channel_layout_describe (&dec_ctx->ch_layout, args + ret, sizeof (args) - ret);
    ret = avfilter_graph_create_filter (&buffersrc_ctx, abuffersrc, "in", args, NULL, filter_graph);
    if (ret < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }

    /* buffer audio sink: to terminate the filter chain. */
    buffersink_ctx = avfilter_graph_alloc_filter (filter_graph, abuffersink, "out");
    if (!buffersink_ctx)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            ret = AVERROR (ENOMEM);
            goto end;
        }

    ret = av_opt_set (buffersink_ctx, "sample_formats", "s16", AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
            goto end;
        }

    ret = av_opt_set (buffersink_ctx, "channel_layouts", "stereo", AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
            goto end;
        }

    ret = avfilter_init_dict (buffersink_ctx, NULL);
    if (ret < 0)
        {
            av_log (NULL, AV_LOG_ERROR, "Cannot initialize audio buffer sink\n");
            goto end;
        }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name = av_strdup ("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name = av_strdup ("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr (filter_graph, filters_descr, &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config (filter_graph, NULL)) < 0)
        goto end;

end:
    avfilter_inout_free (&inputs);
    avfilter_inout_free (&outputs);

    return ret;
}

int
decoder_t::process_frame ()
{
    if (got_eof)
        return AVERROR_EOF;

    int ret = 0;
    av_frame_unref (out_frame);
    do
        {
            if (ret == AVERROR_EOF && !got_eof)
                {
                    got_eof = true;
                    /* signal EOF to the filtergraph */
                    if ((ret = av_buffersrc_add_frame_flags (buffersrc_ctx, NULL, 0)) < 0)
                        {
                            av_log (NULL, AV_LOG_ERROR, "Error while closing the filtergraph\n");
                            break;
                        }
                }

            ret = av_buffersink_get_frame (buffersink_ctx, out_frame);
            if (ret == AVERROR_EOF)
                {
                    if (!out_frame->nb_samples)
                        break;
                    continue;
                }

            if (ret == AVERROR (EAGAIN))
                {
                    // got EAGAIN, feed it
                    ret = receive_frame ();
                    if (ret != AVERROR_EOF && ret < 0)
                        {
                            print_err (ret);
                            break;
                        }
                    continue;
                }

            if (ret < 0)
                {
                    print_err (ret);
                }

            // got frame or error
            break;
        }
    while (1);

    return ret;
}

////////////////////////////////////////////////////////////////////////////////

decoder_t::decoder_t () { init (); }

decoder_t::~decoder_t ()
{
    if (fmt_ctx)
        avformat_close_input (&fmt_ctx);
    if (dec_ctx)
        avcodec_free_context (&dec_ctx);

    if (packet)
        av_packet_free (&packet);
    if (frame)
        av_frame_free (&frame);
    if (out_frame)
        av_frame_free (&out_frame);

    packet = nullptr;
    frame = nullptr;
    out_frame = nullptr;

    fmt_ctx = nullptr;
    dec_ctx = nullptr;

    reset_filters ();
}

void
decoder_t::init ()
{
    filter_descr = "anull";
    audio_stream_index = -1;

    packet = av_packet_alloc ();
    frame = av_frame_alloc ();
    out_frame = av_frame_alloc ();
    got_eof = false;

    fmt_ctx = nullptr;
    dec_ctx = nullptr;

    filter_graph = nullptr;
    buffersink_ctx = nullptr;
    buffersrc_ctx = nullptr;

    if (!is_valid ())
        {
            fprintf (stderr, "Could not allocate frame or packet\n");
        }
}

bool
decoder_t::is_valid () const
{
    return packet && frame && out_frame;
}

int
decoder_t::open (const char *fname)
{
    int ret = open_input_file (fname);

    if (ret < 0 && ret != AVERROR_EOF)
        {
            fprintf (stderr, "Error occurred: %s\n", av_err2str (ret));
        }

    return ret;
}

std::string
decoder_t::get_filter_descr () const
{
    return filter_descr;
}

void
decoder_t::set_filter_descr (const std::string &filters)
{
    filter_descr = filters;
}

int
decoder_t::init_filters ()
{
    reset_filters ();

    std::string fdescr =
#ifdef AUDIO_INPUT_USE_EXCITER
        "aexciter," +
#endif // AUDIO_INPUT_USE_EXCITER
        filter_descr;

    int ret = init_filters (fdescr.c_str ());

    if (ret < 0 && ret != AVERROR_EOF)
        {
            fprintf (stderr, "Error occurred: %s\n", av_err2str (ret));
        }

    return ret;
}

void
decoder_t::reset_filters ()
{
    if (filter_graph)
        avfilter_graph_free (&filter_graph);

    filter_graph = nullptr;
    buffersink_ctx = nullptr;
    buffersrc_ctx = nullptr;
}

int
decoder_t::process_frame (std::vector<uint16_t> &out_vec)
{
    int ret = process_frame ();
    if (ret == AVERROR_EOF || ret < 0)
        return ret;
    out_vec = std::move (frame_to_vec (out_frame));
    return ret;
}

int
decoder_t::seek (int64_t timestamp)
{
    // silent error when unseekable
    if ((fmt_ctx->ctx_flags & AVFMTCTX_UNSEEKABLE) == AVFMTCTX_UNSEEKABLE)
        {
            fprintf (stderr, "File is unseekable\n");
            return 0;
        }

    const auto time_base = fmt_ctx->streams[audio_stream_index]->time_base;
    timestamp = timestamp * time_base.num * time_base.den / 1000;

    int ret;
    int64_t seek_timestamp = timestamp;
    if (fmt_ctx->start_time != AV_NOPTS_VALUE)
        seek_timestamp += fmt_ctx->start_time;

    ret = avformat_seek_file (fmt_ctx, audio_stream_index, INT64_MIN, seek_timestamp, seek_timestamp, 0);
    if (ret < 0)
        {
            av_log (NULL, AV_LOG_WARNING, "could not seek to position %0.3f\n", (double)timestamp / 1000);
        }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//

} // namespace musicat
