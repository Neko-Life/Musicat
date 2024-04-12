#include "musicat/server/routes.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/routes/get_guilds.h"
#include "musicat/server/routes/get_login.h"
#include "musicat/server/routes/get_root.h"
#include "musicat/server/routes/post_login.h"

// !TODO: maybe move this to smt like server_config.h
// a day
#define CORS_VALID_FOR "86400"

namespace musicat::server::routes
{
#ifndef MUSICAT_NO_SERVER

// any route
void
any_any (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    res->writeStatus (http_status_t.NOT_FOUND_404);
    middlewares::write_headers (res, cors_headers);
    res->end ();
}

// cors protocol
void
options_cors (APIResponse *res, APIRequest *req)
{
    auto cors_headers
        = middlewares::cors (res, req,
                             {
                                 { "Access-Control-Max-Age", CORS_VALID_FOR },
                             });

    if (cors_headers.empty ())
        return;

    res->writeStatus (http_status_t.NO_CONTENT_204);
    middlewares::write_headers (res, cors_headers);
    res->end ();
}

inline constexpr const route_handler_t route_handlers[]
    = { { "/*", ROUTE_METHOD_ANY, any_any },
        { "/*", ROUTE_METHOD_OPTIONS, options_cors },

        { "/", ROUTE_METHOD_GET, get_root },
        { "/login", ROUTE_METHOD_GET, get_login },
        { "/login", ROUTE_METHOD_POST, post_login },
        { "/guilds", ROUTE_METHOD_GET, get_guilds },
        { NULL, ROUTE_METHOD_NULL, NULL } };

void
define_routes (APIApp *app)
{
    for (size_t i = 0;
         i < (sizeof (route_handlers) / sizeof (*route_handlers)); i++)
        {
            const route_handler_t *rh = &route_handlers[i];
            if (rh->method == ROUTE_METHOD_NULL)
                break;

            switch (rh->method)
                {
                case ROUTE_METHOD_GET:
                    app->get (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_POST:
                    app->post (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_OPTIONS:
                    app->options (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_ANY:
                    app->any (rh->path, rh->handler);
                    break;
                }
        }
}

#endif
} // musicat::server::routes
