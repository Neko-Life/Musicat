#include "musicat/musicat.h"
#include "musicat/server.h"
#include <chrono>
#include <stdio.h>
#include <uWebSockets/App.h>

#define PORT 9001

namespace musicat
{
namespace server
{
// SSLApp would be <true, true>
// update accordingly
using MCWsApp = uWS::WebSocket<false, true>;

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

    if (d.is_string ())
        {
            const std::string req = d.get<std::string> ();

            if (req == "bot_info")
                {
                    auto bot = get_client_ptr ();
                    if (!bot)
                        {
                            resd["error"] = true;
                            resd["message"] = "bot not running";
                        }
                    else
                        {
                            resd = "here's the data";
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

    /* if (d.is_string ()) */
    {
        /* const std::string res */
    }
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

    app.ws<SocketData> (
        "/*",
        { uWS::CompressOptions (uWS::DEDICATED_COMPRESSOR_4KB
                                | uWS::DEDICATED_COMPRESSOR),
          4 * 1024, 120, 1024 * 1024, nullptr,
          [] (auto *ws) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server OPEN] %lu\n", (uintptr_t)ws);
                  }

              ws->subscribe ("bot_info_update");
          },
          [] (auto *ws, std::string_view msg, uWS::OpCode code) {
              const bool debug = get_debug_state ();

              /* std::string message(msg); */
              /* std::string logmessage = std::string ("`") + message */
              /*                       + "` " + std::to_string (code); */
              if (debug)
                  {
                      std::cerr << "[server MESSAGE] " << msg << '\n';
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
              /* else ws->send (message); */
          },
          [] (auto *ws) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server DRAIN] %lu %u\n",
                               (uintptr_t)ws, ws->getBufferedAmount ());
                  }
          },
          [] (auto *ws) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server PING] %lu\n", (uintptr_t)ws);
                  }
          },
          [] (auto *ws) {
              const bool debug = get_debug_state ();

              if (debug)
                  {
                      fprintf (stderr, "[server PONG] %lu\n", (uintptr_t)ws);
                  }
          },
          [] (auto *ws, int code, std::string_view message) {
              const bool debug = get_debug_state ();
              /* You may access ws->getUserData() here */

              if (debug)
                  {
                      fprintf (stderr, "[server CLOSE] %lu %d\n",
                               (uintptr_t)ws, code);
                      std::cerr << message << '\n';
                  }
          } });

    app.listen (PORT, [] (auto *listen_socket) {
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
