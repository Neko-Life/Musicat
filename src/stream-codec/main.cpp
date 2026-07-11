#include "musicat/stream_codec.h"
#include <ogg/ogg.h>
#include <unistd.h>

#define EXTRACT_ONLY
// #define SEEK_TEST

int
main (const int argc, const char *argv[])
{
    if (argc < 2)
        return -1;

    musicat::stream_codec::ogg_stream_t s (argv[1]);

    int status = 0;
#ifdef EXTRACT_ONLY

#ifdef SEEK_TEST
    bool seeked = false;
#endif // SEEK_TEST

    ogg_page op;
    while ((status = s.get_next_sync_page (op)) == 0)
        {
#ifdef SEEK_TEST
            if (!seeked && s.has_headers_for_seeking)
                {
                    s.seek_to (1250000L);
                    seeked = true;
                }
#endif // SEEK_TEST

            write (STDOUT_FILENO, op.header, op.header_len);
            write (STDOUT_FILENO, op.body, op.body_len);
        }
#else
    ogg_packet op;
    while ((status = s.get_next_packet (op)) == 0)
        write (STDOUT_FILENO, op.packet, op.bytes);
#endif // EXTRACT_ONLY

    return status;
}
