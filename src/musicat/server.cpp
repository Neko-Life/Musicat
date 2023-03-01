#include "musicat/musicat.h"
#include "musicat/server.h"
#include <stdio.h>
#include <uWebSockets/App.h>

#define PORT 9001

namespace musicat
{
namespace server
{
bool running = false;
/*
uWS::App *app_ptr = nullptr;
*/
bool
get_running_state ()
{
    return running;
}
/*
uWS::App *
get_app_ptr ()
{
    return app_ptr;
}
*/
int
run ()
{
    if (running)
        {
            fprintf (stderr, "[server] Instance already running!\n");
            return 1;
        }

    uWS::App app;
    /* app_ptr = &app; */

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
          },
          [] (auto *ws, std::string_view msg, uWS::OpCode code) {
              const bool debug = get_debug_state ();

              std::string message(msg);
              std::string logmessage = std::string ("`") + message
                                    + "` " + std::to_string (code);
              if (debug)
                  {
                      fprintf (stderr, "[server MESSAGE] %s\n",
                               logmessage.c_str ());
                  }
              if (message == "0")
                {
                    ws->send("1");
                }
              else ws->send (message);
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
    return 0;
}

} // server
} // musicat
