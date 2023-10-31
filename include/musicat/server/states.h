#ifndef MUSICAT_SERVER_STATES_H
#define MUSICAT_SERVER_STATES_H

#include "musicat/server.h"
#include "musicat/server/auth.h"
#include <mutex>
#include <string>

namespace musicat::server::states
{

struct recv_body_t
{
    long long ts;
    const char *endpoint;
    std::string id;
    std::string *body;
    APIResponse *res;
    APIRequest *req;
};

struct oauth_timer_t
{
    long long ts;
    int second_sleep;
    std::string val;
    int (*remove_fn) (const std::string &, std::string *);
    bool (*should_break) (const std::string &);
};

extern std::mutex recv_body_cache_m; // EXTERN_VARIABLE

extern std::mutex oauth_timers_m; // EXTERN_VARIABLE

std::string generate_oauth_state (const std::string &redirect = "");

int remove_oauth_state (const std::string &state,
                        std::string *redirect = NULL);

recv_body_t create_recv_body_t (const char *endpoint, const std::string &id,
                                APIResponse *res, APIRequest *req);

/**
 * @brief Should acquire recv_body_cache_m lock before calling
 * keep holding the lock until done with the returned iterator
 */
std::vector<recv_body_t>::iterator
get_recv_body_cache (const recv_body_t &struct_body);

bool is_recv_body_cache_end_iterator (std::vector<recv_body_t>::iterator i);

int store_recv_body_cache (const recv_body_t &struct_body);

void delete_recv_body_cache (std::vector<recv_body_t>::iterator i);

void reserve_recv_body_cache (size_t siz);

int init ();

void set_jwt_verifier_ptr (auth::jwt_verifier_t *ptr);

auth::jwt_verifier_t *get_jwt_verifier_ptr ();

bool oauth_timer_t_is (const oauth_timer_t &t1, const oauth_timer_t &t2);
int register_timer (const oauth_timer_t &t);
int remove_timer (const oauth_timer_t &t);
int check_timers ();
int remove_all_timers ();
oauth_timer_t get_oauth_timer (const std::string &val);

} // musicat::server::states

#endif // MUSICAT_SERVER_STATES_H
