#include "musicat/server/stream.h"

namespace musicat::server::stream
{

/// packet_state_t

/// stream_state_t

[[nodiscard]] packet_state_t &
stream_state_t::new_packet ()
{
    packet_que.resize (packet_que.size () + 1);
    return packet_que.back ();
}

void
stream_state_t::new_packet_done (packet_state_t &packet)
{
    int64_t id;

    do
        id = create_packet_id ();
    while (find_packet (id) != packet_que.end ());

    packet.id = id;
}

[[nodiscard]] stream_state_t::packet_que_t::iterator
stream_state_t::find_packet (int64_t id)
{
    for (auto i = packet_que.begin (); i != packet_que.end (); i++)
        if (i->id == id)
            return i;

    return packet_que.end ();
}

void
stream_state_t::packet_sent (packet_state_t &packet)
{
    if (packet.sent_count++; packet.sent_count >= subscriber_count)
        remove_packet (packet.id);
}

void
stream_state_t::remove_packet (int64_t id)
{
    if (const auto i = find_packet (id); i != packet_que.end ())
        packet_que.erase (i);
}

void
stream_state_t::increment_subscriber ()
{
    subscriber_count++;
    if (subscriber_count == 0)
        {
            subscriber_count--;
            throw std::runtime_error ("WOW TOO MANY SUBSCRIBER!");
        }
}

void
stream_state_t::decrement_subscriber ()
{
    if (subscriber_count == 0)
        return;
    subscriber_count--;
}

// stream_state_t statics

int64_t
stream_state_t::create_packet_id ()
{
    return std::chrono::steady_clock::now ().time_since_epoch ().count ();
}

/// MANAGER stream_state_t

std::mutex ns_mutex; // EXTERN_VARIABLE
stream_states_t states;

stream_state_t &
new_stream_state (const dpp::snowflake &guild_id)
{
    states.resize (states.size () + 1);
    auto &i = states.back ();

    std::lock_guard lk (i.state_m);
    i.guild_id = guild_id;

    return i;
}

void
remove_stream_state (const dpp::snowflake &guild_id)
{
    if (const auto i = find_stream_state (guild_id); i != states.end ())
        states.erase (i);
}

[[nodiscard]] stream_states_t::iterator
find_stream_state (const dpp::snowflake &guild_id)
{
    for (auto i = states.begin (); i != states.end (); i++)
        if (i->guild_id == guild_id)
            return i;

    return states.end ();
}

[[nodiscard]] bool
stream_state_iterator_is_end (const stream_states_t::iterator i)
{
    return i == states.end ();
}

void
subscribe (const dpp::snowflake &guild_id)
{
    if (auto i = find_stream_state (guild_id);
        !stream_state_iterator_is_end (i))
        {
            std::lock_guard lk (i->state_m);
            i->increment_subscriber ();
        }
    else
        {
            // create new stream state to be filled by guild player
            new_stream_state (guild_id);
        }
}

void
unsubscribe (const dpp::snowflake &guild_id)
{
    if (auto i = find_stream_state (guild_id);
        !stream_state_iterator_is_end (i))
        {
            bool should_erase_iterator = false;

            {
                std::lock_guard lk (i->state_m);
                i->decrement_subscriber ();
                should_erase_iterator = i->subscriber_count <= 0;
            }

            if (should_erase_iterator)
                {
                    // delete this stream state
                    // refactor remove_stream_state if this logic should change
                    states.erase (i);
                }
        }
}

void
handle_send_opus (const dpp::snowflake &guild_id, uint8_t *packet,
                  int packet_len)
{
    if (auto i = find_stream_state (guild_id);
        !stream_state_iterator_is_end (i))
        {
            std::lock_guard lk (i->state_m);
            auto &np = i->new_packet ();

            // copy packet to state
            np.packet.assign (packet, packet + packet_len);

            i->new_packet_done (np);
            // return;
        }

    // no subscriber for this server, ignore
}

} // musicat::server::stream
