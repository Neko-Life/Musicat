#include "musicat/stream_codec.h"
#include <fcntl.h>
#include <ogg/ogg.h>
#include <unistd.h>

#define EXTRACT_ONLY
// #define MODE_OPUS

int
main (const int argc, const char *argv[])
{
    if (argc < 2)
        return -1;

    int fd = open (argv[1], 0);
    if (fd < 0)
        {
            perror ("open");
            return -1;
        }

#ifdef MODE_OPUS
    musicat::stream_codec::ogg_stream_t s (fd);
#else
    musicat::stream_codec::ogg_stream_t s (fd, musicat::stream_codec::OGG_STREAM_READ_OGG_PAGE);
#endif

    int status = 0;
#ifdef EXTRACT_ONLY

    ogg_page op = { nullptr, 0, nullptr, 0 };
    while ((status = s.get_next_page (op)) == 0)
        {
            write (STDOUT_FILENO, op.header, op.header_len);
            write (STDOUT_FILENO, op.body, op.body_len);
        }
#else
    ogg_packet op;
    while ((status = s.get_next_packet_ogg (op)) == 0)
        write (STDOUT_FILENO, op.packet, op.bytes);
#endif // EXTRACT_ONLY

    return status;
}
