#include "musicat/server/states.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include <deque>
#include <string>

// in seconds
#define STATE_LIFETIME 600

namespace musicat::server::states
{

std::map<std::string, std::string> _oauth_states = {};
std::mutex _oauth_states_m;

// !TODO: reserve from somewhere?
std::vector<recv_body_t> _recv_body_cache;
std::mutex recv_body_cache_m; // EXTERN_VARIABLE

template <typename T>
typename std::deque<T>::iterator
util_deque_find (std::deque<T> *_deq, T _find)
{
    auto i = _deq->begin ();
    for (; i != _deq->end (); i++)
        {
            if (*i == _find)
                return i;
        }
    return i;
}

int
util_remove_string_deq (const std::string &val, std::deque<std::string> &deq)
{
    int idx = -1;
    int count = 0;
    for (auto i = deq.begin (); i != deq.end ();)
        {
            if (*i == val)
                {
                    deq.erase (i);
                    idx = count;
                    break;
                }
            else
                {
                    i++;
                    count++;
                }
        }

    return idx;
}

int
remove_oauth_state (const std::string &state, std::string *redirect)
{
    std::lock_guard lk (_oauth_states_m);

    auto i = _oauth_states.find (state);
    if (i == _oauth_states.end ())
        return -1;

    if (redirect)
        *redirect = i->second;

    _oauth_states.erase (i);
    return 0;
}

bool
should_break_oauth_states (const std::string &val)
{
    std::lock_guard lk (_oauth_states_m);
    auto i = _oauth_states.find (val);
    return i == _oauth_states.end ();
}

void
util_create_remove_thread (int second_sleep, const std::string &val,
                           int (*remove_fn) (const std::string &,
                                             std::string *),
                           bool (*should_break) (const std::string &) = NULL)
{
    std::thread t ([val, second_sleep, remove_fn, should_break] () {
        thread_manager::DoneSetter tdms;

        int second_slept = 0;

        while (get_running_state () && second_slept < second_sleep)
            {
                if (should_break && should_break (val))
                    break;

                std::this_thread::sleep_for (std::chrono::seconds (1));
                second_slept++;
            }

        remove_fn (val, NULL);
    });

    thread_manager::dispatch (t);
}

inline constexpr const char token[]
    = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

inline constexpr const int token_size = sizeof (token) - 1;

std::string
generate_oauth_state (const std::string &redirect)
{
    std::string state = "";

    std::lock_guard lk (_oauth_states_m);

    do
        {
            int r1 = util::get_random_number ();
            int len = ((r1 > 0) ? (r1 % 81) : 0) + 80;
            state = "";
            state.reserve (len);

            for (int i = 0; i < len; i++)
                {
                    const int r2 = util::get_random_number ();
                    state += token[(r2 > 0) ? (r2 % token_size) : 0];
                }
        }
    while (_oauth_states.find (state) != _oauth_states.end ());

    _oauth_states.insert_or_assign (state, redirect);

    util_create_remove_thread (STATE_LIFETIME, state, remove_oauth_state,
                               should_break_oauth_states);

    return state;
}

recv_body_t
create_recv_body_t (const char *endpoint, const std::string &id,
                    APIResponse *res, APIRequest *req)
{
    long long ts = std::chrono::high_resolution_clock::now ()
                       .time_since_epoch ()
                       .count ();

    return { ts, endpoint, id, nullptr, res, req };
}

std::vector<recv_body_t>::iterator
get_recv_body_cache (const recv_body_t &struct_body)
{
    auto i = _recv_body_cache.begin ();
    while (i != _recv_body_cache.end ())
        {
            if (i->ts == struct_body.ts && i->id == struct_body.id
                && i->endpoint == struct_body.endpoint)
                {
                    return i;
                }
            i++;
        }

    return i;
}

bool
is_recv_body_cache_end_iterator (std::vector<recv_body_t>::iterator i)
{
    return i == _recv_body_cache.end ();
}

int
store_recv_body_cache (const recv_body_t &struct_body)
{
    auto i = get_recv_body_cache (struct_body);
    if (!is_recv_body_cache_end_iterator (i))
        return 1;

    _recv_body_cache.emplace_back (struct_body);

    return 0;
}

void
delete_recv_body_cache (std::vector<recv_body_t>::iterator i)
{
    if (i->body)
        delete i->body;

    _recv_body_cache.erase (i);
}

} // musicat::server::states
