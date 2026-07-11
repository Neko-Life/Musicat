#include "musicat/server/stream.h"
#include <map>

namespace musicat::server::stream
{

struct stream_state_t
{
    dpp::snowflake guild_id;
    bool headers_sent;
};

stream_state_t
create_stream_state (const dpp::snowflake &guild_id)
{
    return { guild_id, false };
}

static std::map<APIResponse *, stream_state_t> subs;
static std::mutex subs_m;

static std::map<dpp::snowflake, std::vector<std::string> > headers_cache;

void
subscribe (APIResponse *res, const dpp::snowflake &guild_id)
{
    std::lock_guard lk (subs_m);
    subs[res] = create_stream_state (guild_id);
}

void
unsubscribe (APIResponse *res, bool aborted)
{
    std::lock_guard lk (subs_m);
    if (!aborted)
        defer ([res] () { res->end (); });
    subs.erase (res);
}

void
unsubscribe (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (subs_m);
    auto i = subs.begin ();
    while (i != subs.end ())
        {
            if (i->second.guild_id != guild_id)
                {
                    i++;
                    continue;
                }
            auto *res = i->first;
            defer ([res] () { res->end (); });
            i = subs.erase (i);
        }
    headers_cache.erase (guild_id);
}

void
shutdown ()
{
    std::lock_guard lk (subs_m);
    auto i = subs.begin ();
    while (i != subs.end ())
        {
            auto *res = i->first;
            defer ([res] () { res->end (); });
            headers_cache.erase (i->second.guild_id);
            i = subs.erase (i);
        }
}

static int
check_for_headers (const dpp::snowflake &guild_id, const std::string &p)
{
    // cache OpusHead and OpusTag headers for late subscribers
    const char *packet = p.data ();
    size_t packet_len = p.size ();

    if (packet_len <= 27)
        return -1;

    int lacing_size = packet[26];
    size_t header_size = 27 + lacing_size;
    if (packet_len <= (header_size + 8))
        return -1;

    const char *payload = packet + header_size;
    if (!memcmp ("OpusHead", payload, 8))
        {
            headers_cache[guild_id] = {};
            headers_cache[guild_id].push_back (p);
        }
    else if (!memcmp ("OpusTags", payload, 8))
        {
            headers_cache[guild_id].push_back (p);
        }
    else
        return -1;

    return 0;
}

void
broadcast (const dpp::snowflake &guild_id, const unsigned char *packet, size_t packet_len)
{
    std::string p{ packet, packet + packet_len };

    defer (
        [guild_id, p] ()
            {
                std::lock_guard lk (subs_m);
                for (auto &[res, s] : subs)
                    {
                        if (guild_id != s.guild_id)
                            continue;

                        if (!s.headers_sent)
                            {
                                for (const std::string &h : headers_cache[guild_id])
                                    res->write (h);
                                s.headers_sent = true;
                            }

                        res->write (p);
                    }

                check_for_headers (guild_id, p);
            });
}

} // musicat::server::stream
