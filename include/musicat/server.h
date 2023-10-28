#ifndef MUSICAT_SERVER_H
#define MUSICAT_SERVER_H

#include <mutex>
#include <string>
#include <uWebSockets/src/App.h>

#define SERVER_WITH_SSL false
#define BOT_AVATAR_SIZE 480

namespace musicat::server
{

#if SERVER_WITH_SSL == true
using APIApp = uWS::SSLApp;
#else
using APIApp = uWS::App;
#endif

using APIResponse = uWS::HttpResponse<SERVER_WITH_SSL>;
using APIRequest = uWS::HttpRequest;

inline constexpr const struct
{
    const char *OK_200 = "200 OK";
    const char *BAD_REQUEST_400 = "400 Bad Request";
    const char *FORBIDDEN_403 = "403 Forbidden";
    const char *NOT_FOUND_404 = "404 Not Found";
    const char *NO_CONTENT_204 = "204 No Content";
    const char *INTERNAL_SERVER_ERROR_500 = "500 Internal Server Error";
} http_status_t;

// always lock this whenever updating states
// EXTERN_VARIABLE
extern std::mutex ns_mutex;

bool get_running_state ();

// this will be handled on `message` event in the client,
// so be sure to use helper function to construct
// whatever data you want to send
int publish (const std::string &topic, const std::string &message);

// shutdown server, returns 0 on success, else 1
int shutdown ();

int run ();

} // musicat::server

#endif // MUSICAT_SERVER_H
