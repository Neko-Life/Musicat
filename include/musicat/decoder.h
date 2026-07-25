#ifndef MUSICAT_DECODER_H
#define MUSICAT_DECODER_H

#include <cstdint>
#include <string>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
}

namespace musicat
{
class decoder_t
{
    std::string filter_descr;

    AVFormatContext *fmt_ctx;
    AVCodecContext *dec_ctx;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;

    AVPacket *packet;
    AVFrame *frame;

  public:
    AVFrame *out_frame;
    bool got_eof;

  private:
    int audio_stream_index;
    int open_input_file (const char *filename);
    int read_frame ();
    int receive_frame ();
    int init_filters (const char *filters_descr);
    int process_frame ();

  public:
    decoder_t ();
    ~decoder_t ();

    void init ();
    bool is_valid () const;
    int open (const char *fname);
    std::string get_filter_descr () const;
    void set_filter_descr (const std::string &filters);
    int init_filters ();
    void reset_filters ();
    int process_frame (std::vector<uint16_t> &out_vec);
    int seek (int64_t timestamp);

    static std::vector<uint16_t>
    frame_to_vec (const AVFrame *frame)
    {
        const int n = frame->nb_samples * frame->ch_layout.nb_channels;
        const uint16_t *p = (uint16_t *)frame->data[0];
        const uint16_t *p_end = p + n;
        return std::vector (p, p_end);
    }

    static void
    print_err (int status)
    {
        if (status < 0 && status != AVERROR_EOF)
            {
                fprintf (stderr, "Error occurred: %s\n", av_err2str (status));
            }
    }
};
} // namespace musicat

#endif // MUSICAT_DECODER_H
