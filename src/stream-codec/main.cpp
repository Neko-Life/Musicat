#include "musicat/decoder.h"
#include "musicat/stream_codec.h"
#include <fcntl.h>
#include <unistd.h>

static int
write_packet (void *opaque, const uint8_t *buf, int buf_size)
{
    if (!buf_size)
        return AVERROR_EOF;

    return fwrite (buf, 1, buf_size, stdout);
}

int
main (const int argc, const char *argv[])
{
    if (argc < 2)
        return -1;

    const char *fname = argv[1];

    // init decoder
    musicat::decoder_t dec;
    if (!dec.is_valid ())
        return -1;
    if (dec.open (fname))
        return -1;
    if (dec.init_filters ())
        return -1;

    musicat::stream_codec::stream_codec_t streamc;

    int ret = 0;

    ret = streamc.init (NULL, &write_packet);
    if (ret)
        return ret;

    while (true)
        {
            AVFrame *frm = nullptr;
            AVPacket *pkt = nullptr;
            while (!pkt && ret != AVERROR_EOF && ret >= 0)
                {
                    while ((ret = streamc.get_packet (&pkt)) == AVERROR (EAGAIN))
                        {
                            ret = dec.process_frame (&frm);
                            if (ret == AVERROR_EOF || ret < 0)
                                {
                                    // signal eof
                                    streamc.write_pcm_frame (NULL);
                                    break;
                                }

                            if (streamc.write_pcm_frame (frm) == AVERROR_EOF)
                                break;
                        }
                }

            if (ret == AVERROR_EOF || ret < 0)
                break;

            // log pkt
            streamc.log_packet (pkt);
            if ((ret = streamc.write_packet (pkt)) != 0)
                break;
        }

    // flush stream
    streamc.end ();

    return ret;
}
