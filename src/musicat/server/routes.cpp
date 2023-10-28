#include "musicat/server/routes.h"
#include "musicat/server.h"
#include "musicat/server/middlewares.h"

// !TODO: maybe move this to smt like server_config.h
// a day
#define CORS_VALID_FOR "86400"

namespace musicat::server::routes
{

// any route
void
any_any (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    res->writeStatus (http_status_t.NOT_FOUND_404)->end ();
}

// cors protocol
void
options_cors (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req,
                                { { "Access-Control-Max-Age", CORS_VALID_FOR },
                                  { "Content-Length", "0" } });

    if (status)
        return;

    res->writeStatus (http_status_t.NO_CONTENT_204)->end ();
}

void
get_root (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    nlohmann::json r = { { "success", true }, { "message", "API running!" } };

    middlewares::set_content_type_json (res);
    res->writeStatus (http_status_t.OK_200)->end (r.dump ());
}

void
get_login (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    // return login link
}

void
post_login (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    // set set-cookie header and end req
}

inline constexpr const route_handler_t route_handlers[]
    = { { "/*", ROUTE_METHOD_ANY, any_any },
        { "/*", ROUTE_METHOD_OPTIONS, options_cors },

        { "/", ROUTE_METHOD_GET, get_root },
        { "/login", ROUTE_METHOD_GET, get_login },
        { "/login", ROUTE_METHOD_POST, post_login },
        { NULL, ROUTE_METHOD_NULL, NULL } };

void
define_routes (APIApp *app)
{
    for (size_t i = 0;
         i < (sizeof (route_handlers) / sizeof (*route_handlers)); i++)
        {
            const route_handler_t *rh = &route_handlers[i];
            if (rh->path == NULL && rh->method == ROUTE_METHOD_NULL
                && rh->handler == NULL)
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

} // musicat::server::routes
