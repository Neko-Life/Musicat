#include "musicat/server/middlewares.h"
#include "musicat/server/ws/player.h"
#include "musicat/util.h"
#include <uWebSockets/src/App.h>

namespace musicat::server::ws::player::events
{

void
upgrade (APIResponse *res, APIRequest *req, struct us_socket_context_t *ctx)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    // check cookie
    std::string user_id = middlewares::validate_token (res, req, cors_headers);
    if (user_id.empty ())
        return;

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
