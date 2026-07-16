#include "musicat/stream_codec.h"
#include "musicat/audio_config.h"
#include <cstdio>
#include <unistd.h>

namespace musicat::stream_codec
{

ogg_stream_t::ogg_stream_t (int _fd, mode_e _mode) : granulepos (0), mode (_mode), fd (_fd) { init (); }

ogg_stream_t::~ogg_stream_t () { reset (); }

ogg_stream_t &
ogg_stream_t::init ()
{
    stream_initialized = false;
    sync_initialized = false;
    got_eos = false;

    header_pages.clear ();

    return *this;
}

ogg_stream_t &
ogg_stream_t::reset ()
{
    clear_sync_state ();
    clear_stream_state ();

    header_pages.clear ();

    fd = -1;
    mode = OGG_STREAM_READ_OPUS_PACKET;
    granulepos = 0;
    got_eos = false;

    return *this;
}

ogg_stream_t &
ogg_stream_t::open (int _fd, mode_e _mode)
{
    reset ();

    fd = _fd;
    mode = _mode;
    granulepos = 0;
    got_eos = false;

    return *this;
}

ogg_stream_t &
ogg_stream_t::init_sync_state ()
{
    if (sync_initialized)
        return *this;

    ogg_sync_init (&oy);
    sync_initialized = true;

    return *this;
}

ogg_stream_t &
ogg_stream_t::clear_sync_state ()
{
    if (!sync_initialized)
        return *this;

    ogg_sync_clear (&oy);
    sync_initialized = false;
    return *this;
}

ogg_stream_t &
ogg_stream_t::init_stream_state (int serialno)
{
    if (stream_initialized)
        return *this;

    ogg_stream_init (&os, serialno);
    stream_initialized = true;

    return *this;
}

ogg_stream_t &
ogg_stream_t::clear_stream_state ()
{
    if (!stream_initialized)
        return *this;

    ogg_stream_clear (&os);
    stream_initialized = false;
    return *this;
}

// !TODO: this doesn't recognize opus headers and tags which doesn't produce valid ogg header page!
int
ogg_stream_t::get_next_page_opus (ogg_page &o)
{
    if (mode != OGG_STREAM_READ_OPUS_PACKET)
        return -1;

    if (got_eos)
        return 1;

    if (!stream_initialized)
        init_stream_state (0);

    int status = 0;
    while (ogg_stream_pageout (&os, &o) == 0)
        {
            if ((status = ogg_stream_check (&os)) != 0)
                return status;

            char buf[OPUS_MAX_ENCODE_OUTPUT_SIZE];
            size_t siz = read (fd, buf, OPUS_MAX_ENCODE_OUTPUT_SIZE);

            long eos = siz < OPUS_MAX_ENCODE_OUTPUT_SIZE ? 1 : 0;
            if (siz)
                {
                    granulepos += siz;

                    ogg_iovec_t iov;
                    iov.iov_base = buf;
                    iov.iov_len = siz;

                    ogg_stream_iovecin (&os, &iov, 1, eos, granulepos);
                }

            if (eos)
                {
                    got_eos = true;
                    ogg_stream_flush (&os, &o);
                    break;
                }
        }

    if (is_header_page (o))
        header_pages.push_back (o);

    return 0;
}

int
ogg_stream_t::get_next_page_ogg (ogg_page &o)
{
    if (mode != OGG_STREAM_READ_OGG_PAGE)
        return -1;

    init_sync_state ();

    while (ogg_sync_pageout (&oy, &o) != 1)
        {
            char *sync_buf = ogg_sync_buffer (&oy, BUFSIZ);
            size_t siz = read (fd, sync_buf, BUFSIZ);

            ogg_sync_wrote (&oy, siz);
            if (siz == 0)
                return 1;
        }

    if (is_header_page (o))
        header_pages.push_back (o);

    return 0;
}

int
ogg_stream_t::get_next_page (ogg_page &o)
{
    if (fd < 0)
        return -1;

    if (mode == OGG_STREAM_READ_OGG_PAGE)
        return get_next_page_ogg (o);

    return get_next_page_opus (o);
}

int
ogg_stream_t::get_next_packet_ogg (ogg_packet &o)
{
    if (mode != OGG_STREAM_READ_OGG_PAGE)
        return -1;

    int status = 0;
    do
        {
            ogg_page og;
            status = get_next_page (og);
            if (status != 0)
                return status;

            if (!stream_initialized)
                init_stream_state (ogg_page_serialno (&og));

            status = ogg_stream_check (&os);
            if (status != 0)
                return status;

            ogg_stream_pagein (&os, &og);

            if (ogg_page_eos (&og))
                got_eos = true;
        }
    while (ogg_stream_packetout (&os, &o) == 0);

    return status;
}

bool
ogg_stream_t::is_header_page (ogg_page &o)
{
    if (!o.body_len || !o.body)
        return false;

    return !memcmp ("OpusTags", o.body, 8) || !memcmp ("OpusHead", o.body, 8);
}

} // musicat::stream_codec
