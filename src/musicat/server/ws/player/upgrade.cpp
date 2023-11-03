#include "musicat/db.h"
#include "musicat/server.h"
#include "musicat/server/auth.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/ws/player.h"
#include "musicat/util.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

inline constexpr const char token_cookie_name[] = "token=";
inline constexpr const size_t token_cookie_name_size
    = (sizeof (token_cookie_name) / sizeof (*token_cookie_name)) - 1;

void
upgrade (APIResponse *res, APIRequest *req, struct us_socket_context_t *ctx)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    // check cookie
    std::string_view cookie = req->getHeader ("cookie");

    size_t i = cookie.find (token_cookie_name);

    if (i == cookie.npos)
        {
            res->writeStatus (http_status_t.UNAUTHORIZED_401);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return;
        }

    size_t t_start = i + token_cookie_name_size;
    std::string_view token
        = cookie.substr (t_start, cookie.find (" ", t_start));

    if (token.empty ())
        {
            res->writeStatus (http_status_t.UNAUTHORIZED_401);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return;
        }

    std::string user_id = auth::verify_jwt_token (std::string (token));

    if (user_id.empty ())
        {
            res->writeStatus (http_status_t.UNAUTHORIZED_401);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return;
        }

    std::string server_id = std::string (req->getParameter (0));
    if (server_id.empty () || util::valid_number (server_id))
        {
            res->writeStatus (http_status_t.BAD_REQUEST_400);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return;
        }

    dpp::guild *guild = dpp::find_guild (server_id);
    if (!guild)
        {
            res->writeStatus (http_status_t.NOT_FOUND_404);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return;
        }

    res->template upgrade<SocketData> (
        { server_id }, req->getHeader ("sec-websocket-key"),
        req->getHeader ("sec-websocket-protocol"),
        req->getHeader ("sec-websocket-extensions"), ctx);
}

} // musicat::server::ws::player::events
