#ifndef MUSICAT_STREAM_CODEC_H
#define MUSICAT_STREAM_CODEC_H

#include "ogg/ogg.h"
#include <vector>

namespace musicat::stream_codec
{

enum ogg_stream_mode_e : uint8_t
{
    // reading non encapsulated, raw opus packets
    OGG_STREAM_READ_OPUS_PACKET,
    // reading ogg encapsulated packets
    OGG_STREAM_READ_OGG_PAGE,
    // !TODO: implement this
    // submit non encapsulated, raw opus packets buffers
    OGG_STREAM_SUBMIT_OPUS_PACKET,
};

class ogg_stream_t
{
    using mode_e = ogg_stream_mode_e;
    ogg_stream_state os;

    ogg_sync_state oy;

    ogg_int64_t granulepos;

    mode_e mode;
    bool stream_initialized;
    bool sync_initialized;
    bool got_eos;

  public:
    // fd to read ogg page/opus packets
    int fd;

    // found opus headers in the current stream
    std::vector<ogg_page> header_pages;

    ogg_stream_t (int fd, mode_e mode = mode_e::OGG_STREAM_READ_OPUS_PACKET);
    ogg_stream_t (mode_e mode = mode_e::OGG_STREAM_SUBMIT_OPUS_PACKET);
    ~ogg_stream_t ();

    ogg_stream_t &init ();
    ogg_stream_t &reset ();
    ogg_stream_t &open (int fd, mode_e mode = mode_e::OGG_STREAM_READ_OPUS_PACKET);

    ogg_stream_t &init_sync_state ();
    ogg_stream_t &clear_sync_state ();

    ogg_stream_t &init_stream_state (int serialno);
    ogg_stream_t &clear_stream_state ();

    // !TODO: implement this
    int get_next_page_submit_opus (ogg_page &o, char *buf, size_t buf_len);

    int get_next_page_opus (ogg_page &o);
    int get_next_page_ogg (ogg_page &o);

    // get next page from ogg_sync_state
    // return 0 on success
    // return 1 on EOF
    int get_next_page (ogg_page &o);

    // only valid for OGG_STREAM_READ_OGG_PAGE mode
    // return 0 on success
    // return 1 on EOF
    int get_next_packet_ogg (ogg_packet &o);

    int seek_to (long byte);

    bool is_header_page (ogg_page &o);
};

} // musicat::stream_codec

#endif // MUSICAT_STREAM_CODEC_H
