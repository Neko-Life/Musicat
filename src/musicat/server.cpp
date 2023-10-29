#include "musicat/server.h"
#include "musicat/musicat.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/routes.h"
#include "musicat/server/ws.h"
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
APIApp *_app_ptr = nullptr;
uWS::Loop *_loop_ptr = nullptr;
us_listen_socket_t *_listen_socket_ptr = nullptr;

////////////////////////////////////////////////////////////////////////////////

/*
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
                *
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
    } *
}
*/
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

            break;

            // ws_event_t::smt: _handle_event(d["d"]);
            // case ws_req_t::bot_info:
            //     {

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

    APIApp app;

#if SERVER_WITH_SSL == true
    // cert and key
    //
    //
#else
#endif

    // initialize global pointers, assign null to these pointer on exit!
    _app_ptr = &app;
    _loop_ptr = uWS::Loop::get ();

    int PORT = get_server_port ();

    middlewares::load_cors_enabled_origin ();

    // define api routes ======================================={

    routes::define_routes (&app);
    ws::define_ws_routes (&app);

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

        _app_ptr->close ();
    });

    fprintf (stderr, "[server] Shutting down callback dispatched\n");

    return 0;
}

} // musicat::server
