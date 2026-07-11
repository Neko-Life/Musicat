#ifndef MUSICAT_STREAM_CODEC_H
#define MUSICAT_STREAM_CODEC_H

#include "ogg/ogg.h"
#include <string>
#include <vector>
#include "opusenc.h"

namespace musicat::stream_codec
{

class ogg_stream_t
{
    ogg_stream_state os;

    ogg_sync_state oy;

    FILE *inf;

    std::vector<ogg_page> header_pages;

    int seek_header_sent;

    bool stream_initialized;
    bool sync_initialized;
    bool need_seek_header;

  public:
    bool has_headers_for_seeking;

    // file path to ogg/opus file
    std::string fpath;

    // max seek byte of inf, only valid when fpath is opened
    long max_seek_byte;

    ogg_stream_t (const std::string &_fpath);
    ~ogg_stream_t ();

    ogg_stream_t &init ();
    ogg_stream_t &reset ();
    ogg_stream_t &open (const std::string &_fpath);

    ogg_stream_t &init_sync_state ();
    ogg_stream_t &clear_sync_state ();

    ogg_stream_t &init_stream_state (int serialno);
    ogg_stream_t &clear_stream_state ();

    int open_infile ();
    ogg_stream_t &close_infile ();

    // get next page from ogg_stream_state
    // return 0 on success
    // int get_next_page(ogg_page &o);

    // get next page from ogg_sync_state
    // return 0 on success
    // return 1 on EOF
    int get_next_sync_page (ogg_page &o);

    // return 0 on success
    // return 1 on EOF
    int get_next_packet (ogg_packet &o);

    int seek_to (long byte);

    bool is_header_page (ogg_page &o);
};

} // musicat::stream_codec

#endif // MUSICAT_STREAM_CODEC_H
