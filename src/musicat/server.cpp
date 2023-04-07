#include "musicat/musicat.h"
#include "musicat/server.h"
#include <chrono>
#include <stdio.h>
#include <uWebSockets/App.h>
/*#include "uWebSockets/AsyncFileReader.h"
#include "uWebSockets/AsyncFileStreamer.h"
#include "uWebSockets/Middleware.h"*/

#define BOT_AVATAR_SIZE 128

namespace musicat
{
namespace server
{
// SSLApp would be <true, true, SocketData>
// update accordingly
using MCWsApp = uWS::WebSocket<false, true, SocketData>;

std::mutex ns_mutex;
bool running = false;

// assign null to these pointer on exit!
uWS::App *_app_ptr = nullptr;
uWS::Loop *_loop = nullptr;

std::deque<std::string> _nonces = {};

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

size_t _remove_nonce (const std::string &nonce);

std::string
_generate_nonce ()
{
    std::string nonce = std::to_string (
        std::chrono::duration_cast<std::chrono::milliseconds> (
            std::chrono::high_resolution_clock::now ().time_since_epoch ())
            .count ());
    _nonces.push_back (nonce);

    std::thread t ([nonce] () {
        std::this_thread::sleep_for (std::chrono::seconds (3));
        _remove_nonce (nonce);
    });
    t.detach ();

    return nonce;
}

size_t
_remove_nonce (const std::string &nonce)
{
    size_t idx = -1;
    size_t count = 0;
    for (auto i = _nonces.begin (); i != _nonces.end ();)
        {
            if (*i == nonce)
                {
                    _nonces.erase (i);
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

    if (d.is_number ())
        {
            const int64_t req = d.get<int64_t> ();

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

                        auto &container = guild_cache->get_container ();

                        for (auto pair : container)
                            {
                                auto *guild = pair.second;
                                if (!guild)
                                    continue;

                                nlohmann::json to_push
                                    = guild->build_json (true);

                                resd.push_back (to_push);
                            }

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
    _loop = uWS::Loop::get ();

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

              if (!msg.length ())
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
                      if (!json_payload.is_object ())
                          {
                              _log_err (
                                  ws,
                                  "[server ERROR] Payload is not an object\n");
                              return;
                          }

                      if (json_payload["type"].is_string ())
                          {
                              nlohmann::json d = json_payload["d"];

                              if (d.is_null ())
                                  {
                                      _log_err (ws,
                                                "[server ERROR] d is null\n");
                                      return;
                                  }

                              const std::string payload_type
                                  = json_payload.value ("type", "");
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
    if (webapp_dir.length ()) {
        AsyncFileStreamer asyncFileStreamer(webapp_dir);

        app.get("/*", [&asyncFileStreamer](auto *res, auto *req) {
                serveFile(res, req);
                asyncFileStreamer.streamFile(res, req->getUrl());
            });
    }
    */
    // define http routes =======================================}

    app.listen (PORT, [PORT] (auto *listen_socket) {
        if (listen_socket)
            {
                fprintf (stderr, "[server] Listening on port %d \n", PORT);
            }
    });

    running = true;
    app.run ();

    // socket exiting, assigning null to these pointer
    _app_ptr = nullptr;
    _loop = nullptr;

    return 0;
}

int
publish (const std::string &topic, const std::string &message)
{
    if (!_app_ptr)
        {
            return 1;
        }

    if (!_loop)
        {
            return 2;
        }

    _loop->defer ([topic, message] () {
        if (!_app_ptr)
            return;

        _app_ptr->publish (topic, message, uWS::OpCode::BINARY);
    });

    return 0;
}

} // server
} // musicat
