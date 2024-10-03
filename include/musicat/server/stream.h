#ifndef MUSICAT_SERVER_STREAM_H
#define MUSICAT_SERVER_STREAM_H

#include <cstdint>
#include <dpp/dpp.h>
#include <opus/opus.h>
#include <opus/opus_types.h>

namespace musicat::server::stream
{

struct packet_state_t
{
    using packet_t = std::vector<opus_int16>;

    int64_t id;
    // !TODO: streaming opus through standard browser request is not possible
    // change this to OGG frame
    packet_t packet;
    // count of subscriber which is done sending this packet
    uint64_t sent_count;
};

struct stream_state_t
{
    using packet_que_t = std::vector<packet_state_t>;

    // should be locked whenever reading/writing to packet_que and its
    // entries or calling any of the method of this struct
    std::mutex state_m;

    dpp::snowflake guild_id;

    packet_que_t packet_que;
    uint64_t subscriber_count;

    stream_state_t () = default;

    // move constructor to allow moving
    stream_state_t (stream_state_t &&o)
    {
        guild_id = std::move (o.guild_id);
        packet_que = std::move (o.packet_que);
        subscriber_count = std::move (o.subscriber_count);
    }

    stream_state_t &
    operator= (stream_state_t &&o)
    {
        guild_id = std::move (o.guild_id);
        packet_que = std::move (o.packet_que);
        subscriber_count = std::move (o.subscriber_count);
        return *this;
    }

    // delete assignment
    stream_state_t &operator= (const stream_state_t &) = delete;

    // should call new_packet_done after done setting up the returned packet
    // might invalidates all existing iterator
    [[nodiscard]] packet_state_t &new_packet ();
    void new_packet_done (packet_state_t &packet);

    [[nodiscard]] packet_que_t::iterator find_packet (int64_t id);

    // might invalidates all existing iterator
    void packet_sent (packet_state_t &packet);
    // invalidates all existing iterator
    void remove_packet (int64_t id);

    void increment_subscriber ();
    void decrement_subscriber ();

    static int64_t create_packet_id ();
};

///

using stream_states_t = std::vector<stream_state_t>;

// should lock this whenever calling any of this namespace function
extern std::mutex ns_mutex; // EXTERN_VARIABLE

// invalidates all existing iterator
stream_state_t &new_stream_state (const dpp::snowflake &guild_id);

// invalidates all existing iterator
void remove_stream_state (const dpp::snowflake &guild_id);

[[nodiscard]] stream_states_t::iterator
find_stream_state (const dpp::snowflake &guild_id);

[[nodiscard]] bool
stream_state_iterator_is_end (const stream_states_t::iterator i);

// might invalidates all existing iterator
void subscribe (const dpp::snowflake &guild_id);

// might invalidates all existing iterator
void unsubscribe (const dpp::snowflake &guild_id);

void handle_send_opus (const dpp::snowflake &guild_id, uint8_t *packet,
                       int packet_len);

} // musicat::server::stream

#endif // MUSICAT_SERVER_STREAM_H
