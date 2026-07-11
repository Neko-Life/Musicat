#include "musicat/stream_codec.h"
#include <cstddef>
#include <cstdio>

namespace musicat::stream_codec
{

ogg_stream_t::ogg_stream_t (const std::string &_fpath) : fpath (_fpath) { init (); }

ogg_stream_t::~ogg_stream_t () { reset (); }

ogg_stream_t &
ogg_stream_t::init ()
{
    stream_initialized = false;
    sync_initialized = false;

    max_seek_byte = 0;
    need_seek_header = false;
    header_pages.clear ();
    seek_header_sent = 0;
    has_headers_for_seeking = false;

    inf = nullptr;

    return *this;
}

ogg_stream_t &
ogg_stream_t::reset ()
{
    clear_sync_state ();
    clear_stream_state ();
    close_infile ();

    return *this;
}

ogg_stream_t &
ogg_stream_t::open (const std::string &_fpath)
{
    reset ();

    fpath = _fpath;

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

ogg_stream_t &
ogg_stream_t::close_infile ()
{
    max_seek_byte = 0;
    need_seek_header = false;
    header_pages.clear ();
    seek_header_sent = 0;
    has_headers_for_seeking = false;

    if (inf)
        fclose (inf);
    inf = nullptr;

    return *this;
}

int
ogg_stream_t::open_infile ()
{
    if (fpath.empty ())
        return -1;

    inf = fopen (fpath.c_str (), "r");
    if (!inf)
        {
            int status = errno;
            perror ("[ogg_stream_t::open_infile ERROR] fopen:");
            return status;
        }

    fseek (inf, 0L, SEEK_END);
    max_seek_byte = ftell (inf);
    rewind (inf);

    return 0;
}

int
ogg_stream_t::get_next_sync_page (ogg_page &o)
{
    if (!inf)
        {
            int status = open_infile ();
            if (status != 0)
                return status;
        }

    if (need_seek_header)
        {
            size_t hsiz = header_pages.size ();
            if (hsiz > (size_t)seek_header_sent)
                o = header_pages.at (seek_header_sent);

            ++seek_header_sent;
            if ((size_t)seek_header_sent >= hsiz)
                {
                    need_seek_header = false;
                    seek_header_sent = 0;
                }
            return 0;
        }

    init_sync_state ();

    while (ogg_sync_pageout (&oy, &o) != 1)
        {
            char *sync_buf = ogg_sync_buffer (&oy, BUFSIZ);
            size_t siz = fread (sync_buf, 1, BUFSIZ, inf);

            ogg_sync_wrote (&oy, siz);
            if (siz == 0)
                return 1;
        }

    if (is_header_page (o))
        header_pages.push_back (o);
    else
        has_headers_for_seeking = true;

    return 0;
}

int
ogg_stream_t::get_next_packet (ogg_packet &o)
{
    int status = 0;
    do
        {
            ogg_page og;
            status = get_next_sync_page (og);
            if (status != 0)
                return status;

            if (!stream_initialized)
                init_stream_state (ogg_page_serialno (&og));

            status = ogg_stream_check (&os);
            if (status != 0)
                return status;

            ogg_stream_pagein (&os, &og);
        }
    while (ogg_stream_packetout (&os, &o) == 0);

    return status;
}

int
ogg_stream_t::seek_to (long byte)
{
    if (byte < 0)
        return -1;

    if (!inf)
        {
            int status = open_infile ();
            if (status != 0)
                return status;
        }

    if (byte > max_seek_byte)
        return -1;

    int status = fseek (inf, byte, SEEK_SET);
    if (status == 0)
        need_seek_header = true;
    return status;
}

bool
ogg_stream_t::is_header_page (ogg_page &o)
{
    return !memcmp ("OpusTags", o.body, 8) || !memcmp ("OpusHead", o.body, 8);
}

} // musicat::stream_codec
