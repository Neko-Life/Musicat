#include "musicat/server/stream.h"
#include <map>

namespace musicat::server::stream
{

struct stream_state_t
{
    APIResponse *res;
    bool headers_sent;
};

struct guild_stream_state_t
{
    std::vector<stream_state_t> streams;
    std::mutex streams_m;

    std::vector<std::string> headers_cache;
    std::vector<std::string> packets_cache;
};

static std::map<dpp::snowflake, guild_stream_state_t> subs;
static std::mutex subs_m;

static guild_stream_state_t &
get_guild_stream_state (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (subs_m);
    guild_stream_state_t &s = subs[guild_id];
    return s;
}

void
subscribe (APIResponse *res, const dpp::snowflake &guild_id)
{
    guild_stream_state_t &state = get_guild_stream_state (guild_id);

    std::lock_guard lk (state.streams_m);
    state.streams.push_back (stream_state_t{ res, false });
}

void
unsubscribe (APIResponse *res, const dpp::snowflake &guild_id, bool aborted)
{
    if (!aborted)
        defer ([res] () { res->end (); });

    guild_stream_state_t &state = get_guild_stream_state (guild_id);

    std::lock_guard lk (state.streams_m);
    auto i = state.streams.begin ();
    while (i != state.streams.end ())
        {
            if (i->res != res)
                {
                    i++;
                    continue;
                }

            state.streams.erase (i);
            break;
        }
}

void
unsubscribe (const dpp::snowflake &guild_id)
{
    defer (
        [guild_id] ()
            {
                {
                    guild_stream_state_t &state = get_guild_stream_state (guild_id);

                    std::lock_guard lk (state.streams_m);
                    for (auto &s : state.streams)
                        s.res->end ();
                    state.streams.clear ();
                }
            });
}

void
shutdown ()
{
    defer (
        [] ()
            {
                std::lock_guard lk (subs_m);
                auto i = subs.begin ();
                while (i != subs.end ())
                    {
                        {
                            std::lock_guard lk (i->second.streams_m);
                            for (auto &s : i->second.streams)
                                s.res->end ();
                        }
                        i = subs.erase (i);
                    }
            });
}

static int
check_for_headers (guild_stream_state_t &state, const std::string &p)
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
            state.headers_cache.clear ();
            state.packets_cache.clear ();
            state.headers_cache.push_back (p);
        }
    else if (!memcmp ("OpusTags", payload, 8))
        {
            state.headers_cache.push_back (p);
        }
    else
        return -1;

    return 0;
}

// cache the last few packets so browsers won't need to wait for too long before playing smoothly
static int
cache_packets (guild_stream_state_t &state, const std::string &p)
{
    state.packets_cache.push_back (p);
    if (state.packets_cache.size () >= 128)
        {
            state.packets_cache.erase (state.packets_cache.begin ());
            return 1;
        }

    return 0;
}

void
broadcast (const dpp::snowflake &guild_id, const unsigned char *packet, size_t packet_len)
{
    std::string p{ packet, packet + packet_len };

    defer (
        [guild_id, p] ()
            {
                guild_stream_state_t &state = get_guild_stream_state (guild_id);
                {
                    std::lock_guard lk (state.streams_m);
                    for (auto &s : state.streams)
                        {
                            if (!s.headers_sent)
                                {
                                    for (const std::string &h : state.headers_cache)
                                        s.res->write (h);

                                    for (const std::string &h : state.packets_cache)
                                        s.res->write (h);

                                    s.headers_sent = true;
                                }

                            s.res->write (p);
                        }
                }

                int is_header = check_for_headers (state, p);
                if (!is_header)
                    cache_packets (state, p);
            });
}

} // musicat::server::stream
