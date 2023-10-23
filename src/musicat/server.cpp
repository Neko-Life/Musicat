#include "musicat/server.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include "yt-search/encode.h"
#include <chrono>
#include <stdio.h>
#include <uWebSockets/src/App.h>
/*#include "uWebSockets/AsyncFileReader.h"
#include "uWebSockets/AsyncFileStreamer.h"
#include "uWebSockets/Middleware.h"*/

#define APP_WITH_SSL false
#define BOT_AVATAR_SIZE 128

namespace musicat
{
namespace server
{
// SSLApp would be <true, true, SocketData>
// update accordingly
using MCWsApp = uWS::WebSocket<APP_WITH_SSL, true, SocketData>;

std::mutex ns_mutex; // EXTERN_VARIABLE
bool running = false;

// assign null to these pointer on exit!
uWS::App *_app_ptr = nullptr;
uWS::Loop *_loop_ptr = nullptr;
us_listen_socket_t *_listen_socket_ptr = nullptr;

std::deque<std::string> _nonces = {};
std::deque<std::string> _oauth_states = {};

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
_util_create_remove_thread (
    const int &second_sleep, const std::string &val,
    std::function<size_t (const std::string &)> remove_fn)
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

std::string
_generate_oauth_state ()
{
    static constexpr char token[]
        = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static constexpr int token_size = sizeof (token) - 1;

    std::string state = "";

    do
        {
            const int r1 = util::get_random_number ();
            const int len = ((r1 > 0) ? (r1 % 31) : 0) + 50;
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

            switch (req)
                {
                case ws_req_t::bot_info:
                    {
                        auto bot = get_client_ptr ();
                        if (!bot)
                            {
                                _set_resd_error (resd, "Bot not running");
                                break;
                            }

                        resd["avatarUrl"] = bot->me.get_avatar_url (
                            BOT_AVATAR_SIZE, dpp::i_webp);
                        resd["username"] = bot->me.username;
                        resd["description"] = get_bot_description ();

                        break;
                    }

                case ws_req_t::server_list:
                    {
                        auto *guild_cache = dpp::get_guild_cache ();
                        if (!guild_cache)
                            {
                                _set_resd_error (resd, "No guild cached");
                                break;
                            }

                        // lock cache mutex for thread safety
                        std::shared_mutex &cache_mutex
                            = guild_cache->get_mutex ();
                        std::lock_guard<std::shared_mutex &> lk (cache_mutex);

                        for (auto &pair : guild_cache->get_container ())
                            {
                                dpp::guild *guild = pair.second;
                                if (!guild)
                                    continue;

                                nlohmann::json to_push;

                                try
                                    {
                                        to_push = nlohmann::json::parse (
                                            guild->build_json (true));
                                    }
                                catch (...)
                                    {
                                        fprintf (
                                            stderr,
                                            "[server::_handle_req ERROR] "
                                            "Error building guild json\n");
                                        continue;
                                    }

                                to_push["icon_url"]
                                    = guild->get_icon_url (512, dpp::i_webp);
                                to_push["banner_url"] = guild->get_banner_url (
                                    1024, dpp::i_webp);
                                to_push["splash_url"] = guild->get_splash_url (
                                    1024, dpp::i_webp);
                                to_push["discovery_splash_url"]
                                    = guild->get_discovery_splash_url (
                                        1024, dpp::i_webp);

                                resd.push_back (to_push);
                            }

                        break;
                    }

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

    uWS::App app;

    // initialize global pointers, assign null to these pointer on exit!
    _app_ptr = &app;
    _loop_ptr = uWS::Loop::get ();

    const int PORT = get_server_port ();

    // define websocket behavior ======================================={
    app.ws<SocketData> (
        "/*",
        { uWS::CompressOptions (uWS::DEDICATED_COMPRESSOR_4KB
                                | uWS::DEDICATED_COMPRESSOR),
          4 * 1024, 120, 1024 * 1024, false, false, false, 0,
          // upgrade
          // !TODO: handle upgrade (validate cookie, auth etc)
          /*[](uWS::HttpResponse<false> * res, uWS::HttpRequest * req){
            std::string_view header_cookie = req->getHeader("cookie");
            if (header_cookie.empty()) return;
          },*/
          nullptr,
          // open
          [] (auto *ws) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server OPEN] %lu\n", (uintptr_t)ws);
                  }

              ws->subscribe ("bot_info_update");
          },
          // message
          [] (auto *ws, std::string_view msg, uWS::OpCode code) {
              const bool debug = get_debug_state ();

              // std::string message(msg);
              // std::string logmessage = std::string ("`") + message
              //                       + "` " + std::to_string (code);
              if (debug)
                  {
                      fprintf (stderr, "[server MESSAGE] %lu %d: %s\n",
                               (uintptr_t)ws, code,
                               std::string (msg).c_str ());
                  }

              if (msg.empty ())
                  return;

              if (msg == "0")
                  {
                      ws->send ("1");
                      return;
                  }

              nlohmann::json json_payload;
              bool is_json = true;

              try
                  {
                      json_payload = nlohmann::json::parse (msg);
                  }
              catch (...)
                  {
                      is_json = false;
                  }

              if (is_json)
                  {
                      // guarantee payload is object
                      if (!json_payload.is_object ())
                          {
                              _log_err (
                                  ws,
                                  "[server ERROR] Payload is not an object\n");
                              return;
                          }

                      const std::string payload_type
                          = json_payload.value ("type", "");

                      if (!payload_type.empty ())
                          {
                              nlohmann::json d = json_payload["d"];

                              if (d.is_null ())
                                  {
                                      _log_err (ws,
                                                "[server ERROR] d is null\n");
                                      return;
                                  }

                              const std::string nonce
                                  = json_payload.value ("nonce", "");

                              // else if train, no return anywere yet
                              // vvvvvvvvvvvvvvvvvv
                              if (payload_type == "req")
                                  {
                                      _handle_req (ws, nonce, d);
                                  }
                              else if (payload_type == "res")
                                  {
                                      _handle_res (ws, nonce, d);
                                  }
                              else if (payload_type == "e")
                                  {
                                      const int64_t event_type
                                          = json_payload.value ("event", -1);

                                      if (event_type == -1)
                                          {
                                              _log_err (
                                                  ws, "[server ERROR] Invalid "
                                                      "event\n");
                                          }
                                      else
                                          {
                                              _handle_event (ws, event_type,
                                                             d);
                                          }
                                  }
                              else
                                  {
                                      _log_err (ws,
                                                "[server ERROR] Unknown "
                                                "payload type: %s\n",
                                                payload_type.c_str ());
                                  }
                              // else if train, no return anywere yet
                              // ^^^^^^^^^^^^^^^^^^
                          } // if json.type is string

                      // !TODO: do smt
                      return;
                  } // if is_json
              // else ws->send (message);
          },
          // drain
          [] (auto *ws) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server DRAIN] %lu %u\n",
                               (uintptr_t)ws, ws->getBufferedAmount ());
                  }
          },
          // ping
          [] (auto *ws, std::string_view msg) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server PING] %lu: %s\n",
                               (uintptr_t)ws, std::string (msg).c_str ());
                  }
          },
          // pong
          [] (auto *ws, std::string_view msg) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server PONG] %lu: %s\n",
                               (uintptr_t)ws, std::string (msg).c_str ());
                  }
          },
          // subscription
          [] (auto *ws, std::string_view topic, int idk1, int idk2) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr,
                               "[server SUBSCRIPTION] %lu, %d, %d: %s\n",
                               (uintptr_t)ws, idk1, idk2,
                               std::string (topic).c_str ());
                  }
          },
          // close
          [] (auto *ws, int code, std::string_view message) {
              const bool debug = get_debug_state ();
              // You may access ws->getUserData() here

              if (debug)
                  {
                      fprintf (stderr, "[server CLOSE] %lu %d: %s\n",
                               (uintptr_t)ws, code,
                               std::string (message).c_str ());
                  }
          } });
    // define websocket behavior =======================================}

    // !TODO
    // define api routes ======================================={
    app.post ("/login",
              [] (uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
                  // find user and verify email password

                  // set set-cookie header and end req
              });

    app.post ("/signup",
              [] (uWS::HttpResponse<false> *res, uWS::HttpRequest *req) {
                  // do signup stuff
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

        us_listen_socket_close (APP_WITH_SSL, _listen_socket_ptr);
        _listen_socket_ptr = nullptr;
    });

    fprintf (stderr, "[server] Shutting down callback dispatched\n");

    return 0;
}

} // server
} // musicat
