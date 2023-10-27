#include "musicat/server.h"
#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include "yt-search/encode.h"
#include <chrono>
#include <stdio.h>
#include <uWebSockets/src/App.h>
/*#include "uWebSockets/AsyncFileReader.h"
#include "uWebSockets/AsyncFileStreamer.h"
#include "uWebSockets/Middleware.h"*/

namespace musicat::server
{
// SSLApp would be <true, true, SocketData>
// update accordingly
using MCWsApp = uWS::WebSocket<SERVER_WITH_SSL, true, ws::player::SocketData>;

std::mutex ns_mutex; // EXTERN_VARIABLE
bool running = false;

// assign null to these pointer on exit!
uWS::App *_app_ptr = nullptr;
uWS::Loop *_loop_ptr = nullptr;
us_listen_socket_t *_listen_socket_ptr = nullptr;

std::deque<std::string> _nonces = {};
std::deque<std::string> _oauth_states = {};

// !TODO: move all these to appropriate files
void
_log_err (MCWsApp *ws, const char *format, ...)
{
    fprintf (stderr, "[server ERROR] ws %lu vvvvvvvvvvv\n", (uintptr_t)ws);

    va_list argptr;
    va_start (argptr, format);
    vfprintf (stderr, format, argptr);
    va_end (argptr);

    fprintf (stderr, "[server ERROR] ws %lu ^^^^^^^^^^^\n", (uintptr_t)ws);
}

template <typename T>
typename std::deque<T>::iterator
_util_deque_find (std::deque<T> *_deq, T _find)
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
_util_remove_string_deq (const std::string &val, std::deque<std::string> &deq)
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

int _remove_nonce (const std::string &nonce);
int _remove_oauth_state (const std::string &state);

void
_util_create_remove_thread (int second_sleep, const std::string &val,
                            int (*remove_fn) (const std::string &))
{
    std::thread t ([val, second_sleep, remove_fn] () {
        thread_manager::DoneSetter tdms;

        std::this_thread::sleep_for (std::chrono::seconds (second_sleep));

        remove_fn (val);
    });

    thread_manager::dispatch (t);
}

std::string
_generate_nonce ()
{
    std::string nonce = std::to_string (
        std::chrono::duration_cast<std::chrono::milliseconds> (
            std::chrono::high_resolution_clock::now ().time_since_epoch ())
            .count ());

    _nonces.push_back (nonce);

    _util_create_remove_thread (3, nonce, _remove_nonce);

    return nonce;
}

int
_remove_nonce (const std::string &nonce)
{
    return _util_remove_string_deq (nonce, _nonces);
}

inline constexpr const char token[]
    = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

inline constexpr const int token_size = sizeof (token) - 1;

std::string
_generate_oauth_state ()
{
    std::string state = "";

    do
        {
            int r1 = util::get_random_number ();
            int len = ((r1 > 0) ? (r1 % 31) : 0) + 50;
            state = "";

            for (int i = 0; i < len; i++)
                {
                    const int r2 = util::get_random_number ();
                    state += token[(r2 > 0) ? (r2 % token_size) : 0];
                }
        }
    while (_util_deque_find<std::string> (&_oauth_states, state)
           != _oauth_states.end ());

    _oauth_states.push_back (state);

    _util_create_remove_thread (600, state, _remove_oauth_state);

    return state;
}

int
_remove_oauth_state (const std::string &state)
{
    return _util_remove_string_deq (state, _oauth_states);
}

void
_request (MCWsApp *ws, nlohmann::json &reqd)
{
    nlohmann::json request;
    request["type"] = "req";
    request["d"] = reqd;
    request["nonce"] = _generate_nonce ();
    ws->send (request.dump ());
}

void
_response (MCWsApp *ws, const std::string &nonce, nlohmann::json &resd)
{
    nlohmann::json response;
    response["type"] = "res";
    response["nonce"] = nonce;
    response["d"] = resd;
    ws->send (response.dump ());
}

void
_set_resd_error (nlohmann::json &resd, const std::string &message)
{
    resd["error"] = true;
    resd["message"] = message;
}

void
_handle_req (MCWsApp *ws, const std::string &nonce, nlohmann::json &d)
{
    if (nonce.length () != 16)
        {
            _log_err (ws, "[server ERROR] Invalid nonce\n");
            return;
        }
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server _handle_req] d:\n%s\n",
                     d.dump ().c_str ());
        }

    nlohmann::json resd;

    nlohmann::json d_type;
    const bool is_d_type_number
        = d.is_object () && (d_type = d["type"]).is_number ();

    nlohmann::json type = is_d_type_number ? d_type : d;
    nlohmann::json data = is_d_type_number ? d.value ("d", nlohmann::json ())
                                           : nlohmann::json ();

    if (type.is_number ())
        {
            const int64_t req = type.get<int64_t> ();

            /*
            switch (req)
                {
                case ws_req_t::oauth_req:
                    {
                        if (!data.is_string ())
                            {
                                _set_resd_error (resd, "Bad request");
                                break;
                            }

                        std::string redirect = data.get<std::string> ();
                        if (redirect.empty ())
                            {
                                _set_resd_error (resd, "Bad request");
                                break;
                            }

                        std::string oauth = get_oauth_link ();
                        if (oauth.empty ())
                            {
                                _set_resd_error (resd, "No OAuth configured");
                                break;
                            }

                        resd = oauth + +"&state=" + _generate_oauth_state ()
                               + "&redirect_uri=" + redirect;
                        break;
                    }
                case ws_req_t::invite_req:
                    {
                        if (!data.is_string ())
                            {
                                _set_resd_error (resd, "Bad request");
                                break;
                            }

                        std::string redirect = data.get<std::string> ();
                        if (redirect.empty ())
                            {
                                _set_resd_error (resd, "Bad request");
                                break;
                            }

                        std::string oauth = get_oauth_invite ();
                        if (oauth.empty ())
                            {
                                _set_resd_error (resd, "No OAuth configured");
                                break;
                            }

                        resd = oauth + +"&state=" + _generate_oauth_state ()
                               + "&redirect_uri=" + redirect;
                        break;
                    }
                }
                */
        }

    _response (ws, nonce, resd);
}

void
_handle_res (MCWsApp *ws, const std::string &nonce, nlohmann::json &d)
{
    if (_remove_nonce (nonce) < 0)
        {
            _log_err (ws, "[server ERROR] _handle_res: timed out\n");
            return;
        }

    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server _handle_res] %s\n", nonce.c_str ());
            fprintf (stderr, "%s\n", d.dump ().c_str ());
        }

    /* if (d.is_string ())
    {
        const std::string res
    } */
}

/**
 * @brief `d` is guaranteed object
 */
void
_handle_event (MCWsApp *ws, const int64_t event, nlohmann::json &d)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server _handle_event] d:\n%s\n",
                     d.dump ().c_str ());
        }

    nlohmann::json resd;

    /*
    switch (event)
        {
        case ws_event_t::oauth:
            auto state_prop = d["state"];
            auto redirect_uri_prop = d["redirect_uri"];
            const std::string code = d.value ("code", "");

            if (!state_prop.is_string () || !redirect_uri_prop.is_string ()
                || code.empty ())
                {
                    _set_resd_error (resd, "Invalid operation");
                    break;
                }

            const std::string state = state_prop.get<std::string> ();

            if (_remove_oauth_state (state) < 0)
                {
                    _set_resd_error (resd, "Unauthorized");
                    break;
                }

            std::string secret = get_sha_secret ();

            if (secret.empty ())
                {
                    _set_resd_error (resd, "No OAuth configured");
                    break;
                }

            const std::string data
                = "code=" + code + "&client_id="
                  + std::to_string (get_sha_id ()) + "&client_secret=" + secret
                  + "&grant_type=" + "authorization_code"
                  + "&redirect_uri=" + redirect_uri_prop.get<std::string> ();

            std::thread t (
                [] (const std::string creds) {
                    thread_manager::DoneSetter tmds;
                    std::ostringstream os;

                    curlpp::Easy req;

                    req.setOpt (curlpp::options::Url (DISCORD_API_URL
                                                      "/oauth2/token"));

                    req.setOpt (curlpp::options::Header (
                        "Content-Type: "
                        "application/x-www-form-urlencoded"));

                    req.setOpt (curlpp::options::PostFields (creds));
                    req.setOpt (
                        curlpp::options::PostFieldSize (creds.length ()));

                    req.setOpt (curlpp::options::WriteStream (&os));

                    try
                        {
                            req.perform ();
                        }
                    catch (const curlpp::LibcurlRuntimeError &e)
                        {
                            fprintf (stderr,
                                     "[ERROR] "
                                     "LibcurlRuntimeError(%d): %s\n",
                                     e.whatCode (), e.what ());

                            return;
                        }

                    // MAGIC INIT
                    const std::string rawhttp = os.str ();

                    fprintf (stderr, "%s\n", creds.c_str ());
                    fprintf (stderr, "%s\n", rawhttp.c_str ());
                },
                data);

            thread_manager::dispatch (t);

            break;

            // ws_event_t::smt: _handle_event(d["d"]);
            // case ws_req_t::bot_info:
            //     {
            //         auto bot = get_client_ptr ();
            //         if (!bot)
            //             {
            //                 _set_resd_error (resd, "Bot not running");
            //                 break;
            //             }

            //         resd["avatarUrl"] = bot->me.get_avatar_url (
            //             BOT_AVATAR_SIZE, dpp::i_webp);
            //         resd["username"] = bot->me.username;
            //         resd["description"] = get_bot_description ();

            //         break;
            //     }

            // case ws_req_t::server_list:
            //     {
            //         auto *guild_cache = dpp::get_guild_cache ();
            //         if (!guild_cache)
            //             {
            //                 _set_resd_error (resd, "No guild cached");
            //                 break;
            //             }

            //         // lock cache mutex for thread safety
            //         std::shared_mutex &cache_mutex
            //             = guild_cache->get_mutex ();
            //         std::lock_guard<std::shared_mutex &> lk (cache_mutex);

            //         auto &container = guild_cache->get_container ();

            //         for (auto pair : container)
            //             {
            //                 auto *guild = pair.second;
            //                 if (!guild)
            //                     continue;

            //                 nlohmann::json to_push;

            //                 try
            //                     {
            //                         to_push = nlohmann::json::parse (
            //                             guild->build_json (true));
            //                     }
            //                 catch (...)
            //                     {
            //                         fprintf (
            //                             stderr,
            //                             "[server::_handle_req ERROR] "
            //                             "Error building guild json\n");
            //                         continue;
            //                     }

            //                 resd.push_back (to_push);
            //             }

            //         break;
            //     }

            // case ws_req_t::oauth_state:
            //     {
            //         std::string oauth_state = get_oauth_link ();
            //         if (!oauth_state.length ())
            //             {
            //                 _set_resd_error (resd, "No OAuth configured");
            //                 break;
            //             }

            //         resd = oauth_state
            //                + "&state=" + _generate_oauth_state ();
            //         break;
            //     }
        }
*/

    /* fprintf (stderr, "%s\n", resd.dump (2).c_str ()); */
    /* if (emit) _emit_event (ws, event_name, resd); */
}

bool
get_running_state ()
{
    return running;
}

int
run ()
{
    if (running)
        {
            fprintf (stderr, "[server ERROR] Instance already running!\n");
            return 1;
        }

#if SERVER_WITH_SSL == true
    uWS::SSLApp app;

    // cert and key
    //
    //
#else
    uWS::App app;
#endif

    // initialize global pointers, assign null to these pointer on exit!
    _app_ptr = &app;
    _loop_ptr = uWS::Loop::get ();

    int PORT = get_server_port ();

    app.ws<ws::player::SocketData> ("/ws/player/:server_id",
                                    ws::player::get_behavior ());

    app.get ("/", [] (uWS::HttpResponse<SERVER_WITH_SSL> *res,
                      uWS::HttpRequest *req) {
        nlohmann::json r
            = { { "success", true }, { "message", "API running!" } };

        res->writeStatus ("200 OK")->end (r.dump ());
    });

    // define api routes ======================================={
    app.post ("/login", [] (uWS::HttpResponse<SERVER_WITH_SSL> *res,
                            uWS::HttpRequest *req) {
        // find user and verify email password

        // set set-cookie header and end req
    });

    app.post ("/signup", [] (uWS::HttpResponse<SERVER_WITH_SSL> *res,
                             uWS::HttpRequest *req) {
        // do signup stuff
    });

    app.any ("/*", [] (uWS::HttpResponse<SERVER_WITH_SSL> *res,
                       uWS::HttpRequest *req) {
        res->writeStatus ("404 Not Found")->end ();
    });

    // serve webapp
    /* !TODO: this is not working, need fix
    std::string webapp_dir = get_webapp_dir ();
    if (!webapp_dir.empty ()) {
        AsyncFileStreamer asyncFileStreamer(webapp_dir);

        app.get("/*", [&asyncFileStreamer](auto *res, auto *req) {
                serveFile(res, req);
                asyncFileStreamer.streamFile(res, req->getUrl());
            });
    }
    */
    // define http routes =======================================}

    app.listen (PORT, [PORT] (us_listen_socket_t *listen_socket) {
        if (listen_socket)
            {
                _listen_socket_ptr = listen_socket;
                fprintf (stderr, "[server] Listening on port %d \n", PORT);
            }
        else
            fprintf (stderr, "[server ERROR] Listening socket is null\n");
    });

    running = true;

    app.run ();

    running = false;

    // socket exiting, assigning null to these pointer
    _listen_socket_ptr = nullptr;
    _loop_ptr = nullptr;
    _app_ptr = nullptr;

    return 0;
}

int
publish (const std::string &topic, const std::string &message)
{
    if (!_app_ptr)
        {
            return 1;
        }

    if (!_loop_ptr)
        {
            return 2;
        }

    _loop_ptr->defer ([topic, message] () {
        if (!_app_ptr)
            {
                fprintf (
                    stderr,
                    "[server::publish ERROR] _app_ptr is null on callback\n");
                fprintf (stderr, "Topic:\n%s\n\n", topic.c_str ());
                fprintf (stderr, "Message:\n%s\n\n", message.c_str ());
                return;
            }

        _app_ptr->publish (topic, message, uWS::OpCode::BINARY);
    });

    return 0;
}

int
shutdown ()
{
    if (!_app_ptr)
        {
            return 1;
        }

    if (!_loop_ptr)
        {
            return 2;
        }

    if (!running)
        {
            return 3;
        }

    _loop_ptr->defer ([] () {
        if (!running)
            {
                fprintf (stderr, "[server::shutdown ERROR] server "
                                 "already not running on callback\n");
                return;
            }

        if (!_app_ptr)
            {
                fprintf (stderr, "[server::shutdown ERROR] _app_ptr "
                                 "is null on callback\n");
                return;
            }

        fprintf (stderr, "[server] Shutting down...\n");

        us_listen_socket_close (SERVER_WITH_SSL, _listen_socket_ptr);
        _listen_socket_ptr = nullptr;
    });

    fprintf (stderr, "[server] Shutting down callback dispatched\n");

    return 0;
}

} // musicat::server
