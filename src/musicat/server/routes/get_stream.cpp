#include "musicat/server/routes/get_stream.h"
#include "musicat/server.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/stream.h"
#include <cstdio>
#include <dpp/dpp.h>

namespace musicat::server::routes
{

struct streaming_state_t
{
    APIResponse *res;
    dpp::snowflake guild_id;

    std::vector<int64_t> done_packet_ids;
    int64_t current_processing_id;
    // might not needed? make sure n remove if confirmed
    uint64_t current_processing_offset;

    void
    start_next (int64_t next_packet_id)
    {
        if (current_processing_id != -1
            && !packet_is_done (current_processing_id))
            done_packet_ids.push_back (current_processing_id);

        current_processing_id = next_packet_id;
        current_processing_offset = 0;
    }

    void
    sync_with_stream_state (stream::stream_state_t &stream_state)
    {
        auto i = done_packet_ids.begin ();
        while (i != done_packet_ids.end ())
            if (auto j = stream_state.find_packet (*i);
                j == stream_state.packet_que.end ())
                i = done_packet_ids.erase (i);
            else
                i++;
    }

    bool
    is_done_streaming_all_packet_from (stream::stream_state_t &stream_state)
    {
        // cleanup missing ids first
        sync_with_stream_state (stream_state);

        // check if done_packet_ids includes all packet in the stream_state
        for (auto i = stream_state.packet_que.begin ();
             i != stream_state.packet_que.end (); i++)
            if (!packet_is_done (i->id))
                return false;

        return true;
    }

    bool
    packet_is_done (int64_t id) const
    {
        for (auto i = done_packet_ids.begin (); i != done_packet_ids.end ();
             i++)
            if (*i == id)
                return true;

        return false;
    }
};

/// for this endpoint no need for mutex as uWebSockets guarantee only run on 1
/// thread synchronously
using streaming_states_t = std::vector<streaming_state_t>;
streaming_states_t streaming_states;

streaming_state_t &
new_streaming_state (APIResponse *res, const dpp::snowflake &guild_id)
{
    streaming_states.resize (streaming_states.size () + 1);
    auto &i = streaming_states.back ();

    i.res = res;
    i.guild_id = guild_id;

    i.current_processing_id = -1;
    i.current_processing_offset = 0;

    return i;
}

streaming_states_t::iterator
find_streaming_state (APIResponse *res, const dpp::snowflake &guild_id)
{
    for (auto i = streaming_states.begin (); i != streaming_states.end (); i++)
        if (i->res == res && i->guild_id == guild_id)
            return i;

    return streaming_states.end ();
}

void
destroy_streaming_state (APIResponse *res, const dpp::snowflake &guild_id)
{
    if (auto i = find_streaming_state (res, guild_id);
        i != streaming_states.end ())
        {
            std::lock_guard lk (stream::ns_mutex);
            stream::unsubscribe (guild_id);

            streaming_states.erase (i);
        }
}

int
stream_ogg_page (APIResponse *res, ogg_page *packet)
{
    constexpr size_t max_buf_siz = 64 * 1024;
    static char buf[max_buf_siz];

    // buffer for streaming the ogg page
    const size_t packet_siz
        = (static_cast<size_t> (packet->header_len) + packet->body_len);
    // an ogg page is guaranteed to have less than 64KB in size
    // https://www.xiph.org/ogg/doc/libogg/ogg_page.html
    assert ((sizeof (buf) / sizeof (*buf)) >= packet_siz);

    memcpy (buf, packet->header, packet->header_len);
    memcpy (buf + packet->header_len, packet->body, packet->body_len);

    if (!res->write ({ static_cast<const char *> (buf), packet_siz }))
        // failed
        return 1;

    return 0;
}

bool
handle_streaming (APIResponse *res, const dpp::snowflake &guild_id,
                  uintmax_t offset)
{
    // check if musicat still actually in this guild
    dpp::guild *guild
        = guild_id.empty () ? nullptr : dpp::find_guild (guild_id);
    if (guild == nullptr)
        {
            res->end ();

            destroy_streaming_state (res, guild_id);
            return true;
        }

    std::lock_guard lk_ns (stream::ns_mutex);

    // check if this guild has packet waiting to be sent
    auto packets_stream_state = stream::find_stream_state (guild_id);
    if (stream::stream_state_iterator_is_end (packets_stream_state))
        // no pending packet for now, keep connection alive
        return true;

    std::lock_guard lk_s (packets_stream_state->state_m);
    if (packets_stream_state->packet_que.empty ())
        // no pending packet for now, keep connection alive
        return true;

    // get current streaming state
    auto streaming_state_iterator = find_streaming_state (res, guild_id);

    streaming_state_t &streaming_state
        = streaming_state_iterator == streaming_states.end ()
              ? new_streaming_state (res, guild_id)
              : *streaming_state_iterator;

    // write as many packet as possible
    stream::stream_state_t::packet_que_t::iterator packet;
    // init
    if (streaming_state.current_processing_id == -1)
        {
            packet = packets_stream_state->packet_que.begin ();
            streaming_state.start_next (packet->id);
        }
    else
        {
            packet = packets_stream_state->find_packet (
                streaming_state.current_processing_id);

            if (packet == packets_stream_state->packet_que.end ())
                {
                    // start from the beginning if
                    // current_processing_id was not found
                    packet = packets_stream_state->packet_que.begin ();
                    streaming_state.start_next (packet->id);
                }
        }

    // just iterate until end
    while (packet != packets_stream_state->packet_que.end ()
           && packet->ready_to_send
           && !streaming_state.is_done_streaming_all_packet_from (
               *packets_stream_state))
        {
            if (stream_ogg_page (res, &packet->packet) != 0)
                break;

            // succeed writing this one, continue
            packet = packets_stream_state->packet_sent (packet);
            streaming_state.start_next (
                packet != packets_stream_state->packet_que.end () ? packet->id
                                                                  : -1);
        }

    // end of current write
    return true;
}

void
send_to_all_streaming_state (const dpp::snowflake &guild_id, uint8_t *packet,
                             int len)
{
    std::vector<unsigned char> p (packet, packet + len);

    defer ([p, guild_id] () {
        for (auto i = streaming_states.begin (); i != streaming_states.end ();
             i++)
            {
                if (i->guild_id == guild_id)
                    i->res->write (
                        { reinterpret_cast<const char *> (p.data ()),
                          p.size () });
            }
    });
}

/// actual endpoint handler
void
get_stream (APIResponse *res, APIRequest *req)
{
    auto res_headers = middlewares::cors (res, req);
    if (res_headers.empty ())
        return;

    dpp::snowflake guild_id (req->getParameter (0));

    dpp::guild *guild
        = guild_id.empty () ? nullptr : dpp::find_guild (guild_id);

    if (guild == nullptr)
        {
            response::end_t endres (res);
            endres.status = http_status_t.NOT_FOUND_404;
            endres.headers = res_headers;

            return;
        }

    ///
    res_headers.push_back (
        std::make_pair ("Content-Type", "audio/ogg; codecs=opus"));
    res_headers.push_back (std::make_pair ("Connection", "close"));
    res_headers.push_back (std::make_pair ("Cache-Control", "no-cache"));
    res_headers.push_back (std::make_pair ("Pragma", "no-cache"));

    res->writeStatus (http_status_t.OK_200);
    middlewares::write_headers (res, res_headers);

    // subscribe to the guild stream state
    {
        std::lock_guard lk (stream::ns_mutex);
        stream::subscribe (guild_id);
    }

    handle_streaming (res, guild_id, 0);

    res->onWritable ([res, guild_id] (uintmax_t offset) {
           return handle_streaming (res, guild_id, offset);
       })
        ->onAborted (
            [res, guild_id] () { destroy_streaming_state (res, guild_id); });
}

} // musicat::server::routes
