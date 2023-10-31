#include "musicat/server/states.h"
#include "musicat/musicat.h"
#include "musicat/server/auth.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include <deque>
#include <string>

// in seconds
#define STATE_LIFETIME 600

namespace musicat::server::states
{
// this should always be used inside server thread, hence no mutex
auth::jwt_verifier_t *_jwt_verifier = nullptr;

std::map<std::string, std::string> _oauth_states;
std::mutex _oauth_states_m;

// !TODO: reserve from somewhere?
std::vector<recv_body_t> _recv_body_cache;
std::mutex recv_body_cache_m; // EXTERN_VARIABLE

std::vector<oauth_timer_t> _oauth_timers;
std::mutex oauth_timers_m; // EXTERN_VARIABLE

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
    register_timer ({ std::chrono::high_resolution_clock::now ()
                          .time_since_epoch ()
                          .count (),
                      second_sleep, val, remove_fn, should_break });
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
    if (i == _recv_body_cache.end ())
        return;

    if (i->body)
        delete i->body;

    _recv_body_cache.erase (i);
}

void
reserve_recv_body_cache (size_t siz)
{
    _recv_body_cache.reserve (siz);
}

int
init ()
{
    reserve_recv_body_cache (100);

    return 0;
}

void
set_jwt_verifier_ptr (auth::jwt_verifier_t *ptr)
{
    _jwt_verifier = ptr;
}

auth::jwt_verifier_t *
get_jwt_verifier_ptr ()
{
    return _jwt_verifier;
}

bool
oauth_timer_t_is (const oauth_timer_t &t1, const oauth_timer_t &t2)
{
    return t1.ts == t2.ts && t1.val == t2.val && t1.remove_fn == t2.remove_fn
           && t1.second_sleep == t2.second_sleep
           && t1.should_break == t2.should_break;
}

int
register_timer (const oauth_timer_t &t)
{
    std::lock_guard lk (oauth_timers_m);

    int status = 0;

    auto i = _oauth_timers.begin ();
    while (i != _oauth_timers.end ())
        {
            if (!oauth_timer_t_is (t, *i))
                {
                    i++;
                    continue;
                }

            status = -1;
            break;
        }

    if (status)
        return status;

    _oauth_timers.push_back (t);

    return 0;
}

int
remove_timer (const oauth_timer_t &t)
{
    std::lock_guard lk (oauth_timers_m);

    int status = -1;

    auto i = _oauth_timers.begin ();
    while (i != _oauth_timers.end ())
        {
            if (!oauth_timer_t_is (t, *i))
                {
                    i++;
                    continue;
                }

            status = 0;
            i->remove_fn (i->val, NULL);
            i = _oauth_timers.erase (i);
        }

    return status;
}

int
check_timers ()
{
    std::lock_guard lk (oauth_timers_m);

    int status = -1;

    auto i = _oauth_timers.begin ();
    while (i != _oauth_timers.end ())
        {
            if (i->should_break && i->should_break (i->val))
                {
                    i = _oauth_timers.erase (i);
                    continue;
                }

            long long current_ts = std::chrono::high_resolution_clock::now ()
                                       .time_since_epoch ()
                                       .count ();

            if ((current_ts - i->ts)
                > (long long)(i->second_sleep * 1000000000))
                {
                    i->remove_fn (i->val, NULL);
                    i = _oauth_timers.erase (i);
                    continue;
                }

            i++;
        }

    return status;
}

int
remove_all_timers ()
{
    std::lock_guard lk (oauth_timers_m);

    auto i = _oauth_timers.begin ();
    while (i != _oauth_timers.end ())
        {
            i->remove_fn (i->val, NULL);
            i = _oauth_timers.erase (i);
        }

    return 0;
}

oauth_timer_t
get_oauth_timer (const std::string &val)
{
    std::lock_guard lk (oauth_timers_m);

    for (const auto &i : _oauth_timers)
        {
            if (i.val != val)
                continue;

            return i;
        }

    return { 0, 0, "", NULL, NULL };
}

} // musicat::server::states
