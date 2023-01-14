#ifndef YT_TRACK_INFO_H
#define YT_TRACK_INFO_H

#include "nlohmann/json.hpp"
#include <string>

// {
//   "itag": 251,
//   "url":
//   "https://rr4---sn-2uuxa3vh-n0ck.googlevideo.com/videoplayback?expire=1652100515\u0026ei=Q7l4YvOCFsqk4gL8sjU\u0026ip=180.253.167.236\u0026id=o-AGg8ZbiURngvZ0w2egxnn91JpCOt9Xzqw2iGwD_DKF49\u0026itag=251\u0026source=youtube\u0026requiressl=yes\u0026mh=a0\u0026mm=31%2C29\u0026mn=sn-2uuxa3vh-n0ck%2Csn-npoe7ned\u0026ms=au%2Crdu\u0026mv=m\u0026mvi=4\u0026pl=21\u0026initcwndbps=311250\u0026spc=4ocVC4RqxHKnq_v7bJX7x5L50QbA\u0026vprv=1\u0026mime=audio%2Fwebm\u0026ns=CTmLmEJ16OoQ23ns10X1Ek0G\u0026gir=yes\u0026clen=3779737\u0026dur=230.141\u0026lmt=1540663744967052\u0026mt=1652078448\u0026fvip=4\u0026keepalive=yes\u0026fexp=24001373%2C24007246\u0026c=WEB\u0026txp=5411222\u0026n=9hSwd89k3RQpnFLZfon\u0026sparams=expire%2Cei%2Cip%2Cid%2Citag%2Csource%2Crequiressl%2Cspc%2Cvprv%2Cmime%2Cns%2Cgir%2Cclen%2Cdur%2Clmt\u0026sig=AOq0QJ8wRQIgHYq5UrfLZRfllrLq982PMBO6A1w3x2RkSprctYRuB7wCIQDtWJ-ZeMzyZgJ-zXyuWj30Vy_9Y_8qNbnRvTa0_9yCUQ%3D%3D\u0026lsparams=mh%2Cmm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpl%2Cinitcwndbps\u0026lsig=AG3C_xAwRAIgXvep23EfmatTLa4kmFeauhquhNZZpm9h1p4jv8GsZKoCID5DhifIoG3quuYTA7ulko32MPK-4sGftXJPRP-YOu8G",
//   "mimeType": "audio/webm; codecs=\"opus\"",
//   "bitrate": 143353,
//   "initRange": {
//     "start": "0",
//     "end": "258"
//   },
//   "indexRange": {
//     "start": "259",
//     "end": "663"
//   },
//   "lastModified": "1540663744967052",
//   "contentLength": "3779737",
//   "quality": "tiny",
//   "projectionType": "RECTANGULAR",
//   "averageBitrate": 131388,
//   "audioQuality": "AUDIO_QUALITY_MEDIUM",
//   "approxDurationMs": "230141",
//   "audioSampleRate": "48000",
//   "audioChannels": 2,
//   "loudnessDb": 11.117057
// }

// {
//   "itag": 136,
//   "url":
//   "https://rr4---sn-2uuxa3vh-n0ck.googlevideo.com/videoplayback?expire=1652100515\u0026ei=Q7l4YvOCFsqk4gL8sjU\u0026ip=180.253.167.236\u0026id=o-AGg8ZbiURngvZ0w2egxnn91JpCOt9Xzqw2iGwD_DKF49\u0026itag=136\u0026aitags=133%2C134%2C135%2C136%2C137%2C160%2C242%2C243%2C244%2C247%2C248%2C278%2C394%2C395%2C396%2C397%2C398%2C399\u0026source=youtube\u0026requiressl=yes\u0026mh=a0\u0026mm=31%2C29\u0026mn=sn-2uuxa3vh-n0ck%2Csn-npoe7ned\u0026ms=au%2Crdu\u0026mv=m\u0026mvi=4\u0026pl=21\u0026initcwndbps=311250\u0026spc=4ocVC4RqxHKnq_v7bJX7x5L50QbA\u0026vprv=1\u0026mime=video%2Fmp4\u0026ns=CTmLmEJ16OoQ23ns10X1Ek0G\u0026gir=yes\u0026clen=13927458\u0026dur=230.129\u0026lmt=1540661926964356\u0026mt=1652078448\u0026fvip=4\u0026keepalive=yes\u0026fexp=24001373%2C24007246\u0026c=WEB\u0026txp=5432432\u0026n=9hSwd89k3RQpnFLZfon\u0026sparams=expire%2Cei%2Cip%2Cid%2Caitags%2Csource%2Crequiressl%2Cspc%2Cvprv%2Cmime%2Cns%2Cgir%2Cclen%2Cdur%2Clmt\u0026sig=AOq0QJ8wRQIhANv9afSdY46JTOoJdGU9movv96gzJK5AO7kMVCCIO5dLAiB19T9upMbHzBiE8sVuAX4huNs5P72vMkbta5KZP_COEQ%3D%3D\u0026lsparams=mh%2Cmm%2Cmn%2Cms%2Cmv%2Cmvi%2Cpl%2Cinitcwndbps\u0026lsig=AG3C_xAwRAIgXvep23EfmatTLa4kmFeauhquhNZZpm9h1p4jv8GsZKoCID5DhifIoG3quuYTA7ulko32MPK-4sGftXJPRP-YOu8G",
//   "mimeType": "video/mp4; codecs=\"avc1.4d401f\"",
//   "bitrate": 1389098,
//   "width": 1280,
//   "height": 720,
//   "initRange": {
//     "start": "0",
//     "end": "712"
//   },
//   "indexRange": {
//     "start": "713",
//     "end": "1272"
//   },
//   "lastModified": "1540661926964356",
//   "contentLength": "13927458",
//   "quality": "hd720",
//   "fps": 30,
//   "qualityLabel": "720p",
//   "projectionType": "RECTANGULAR",
//   "averageBitrate": 484161,
//   "approxDurationMs": "230129"
// }
namespace yt_search
{
struct mime_type_t
{
    std::string str;
    std::string type;
    std::string format;
    std::string codecs;

    mime_type_t ();
    mime_type_t (std::string str);
    ~mime_type_t ();
};

struct audio_info_t
{
    nlohmann::json raw;

    // Format code
    int itag ();

    // Stream url
    std::string url ();

    // Metadata
    mime_type_t mime_type ();
    size_t bitrate ();

    // start, end
    std::pair<size_t, size_t> init_range ();

    // start,end
    std::pair<size_t, size_t> index_range ();
    time_t last_modified ();

    // Content length
    size_t length ();
    std::string quality ();
    std::string projection_type ();
    size_t average_bitrate ();
    std::string audio_quality ();

    // Duration in ms
    uint64_t duration ();

    // Audio sampling rate
    uint64_t sample_rate ();

    // Audio channel
    uint8_t channel ();

    // Loudness in decible
    double loudness ();
};

// struct video_info_t {};

struct YTInfo
{
    nlohmann::json raw;

    /**
     * @brief Get audio info
     *
     * @param format Audio format
     * @return audio_info_t
     */
    audio_info_t audio_info (int format);

    // video_info_t video_info(int format);
};

YTInfo get_track_info (std::string url);
}

#endif
