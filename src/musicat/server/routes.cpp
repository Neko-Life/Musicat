#include "musicat/server/routes.h"
#include "musicat/server.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/routes/get_guilds.h"
#include "musicat/server/routes/get_invite.h"
#include "musicat/server/routes/get_login.h"
#include "musicat/server/routes/get_root.h"
#include "musicat/server/routes/get_stream.h"
#include "musicat/server/routes/post_login.h"
#include "musicat/server/routes/post_logout.h"

// !TODO: maybe move this to smt like server_config.h
// a day
#define CORS_VALID_FOR "86400"

namespace musicat::server::routes
{

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
    auto cors_headers = middlewares::cors (res, req,
                                           {
                                               { "Access-Control-Max-Age", CORS_VALID_FOR },
                                           });

    if (cors_headers.empty ())
        return;

    res->writeStatus (http_status_t.NO_CONTENT_204);
    middlewares::write_headers (res, cors_headers);
    res->end ();
}

inline std::map<std::string, time_t> last_reqs;

// !TODO: implement this 304 status properly!
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Status/304
int
one_second_304 (APIResponse *res, APIRequest *req)
{
    auto addr = res->getRemoteAddressAsText ();
    if (addr.empty ())
        return -1;

    time_t cur_ts = time (NULL);

    const std::string rid = std::string (addr) + ";" + std::string (req->getMethod ()) + ";" + std::string (req->getUrl ());

    /* const std::string rid2 = std::string (addr) + ";" */
    /*                          + std::string (req->getMethod ()) + ";" */
    /*                          + std::string (req->getFullUrl ()); */

    /* std::cout << rid << "\n" << rid2 << "\n"; */

    auto last_r = last_reqs.find (rid);
    if (last_r != last_reqs.end ())
        {
            if (last_r->second == cur_ts)
                {
                    // burst request, return 304
                    res->writeStatus (http_status_t.NOT_MODIFIED_304);
                    res->end ();
                    return 0;
                }
            else
                {
                    last_r->second = cur_ts;
                }
        }
    else
        {
            last_reqs.try_emplace (rid, cur_ts);
        }

    return -1;
}

// only for GET and HEAD req
std::function<void (APIResponse *, APIRequest *)>
with_one_second_304 (const route_handler_t *rh)
{
    return [rh] (APIResponse *res, APIRequest *req)
        {
            if (one_second_304 (res, req) == 0)
                return;

            rh->handler (res, req);
        };
}

inline constexpr const route_handler_t route_handlers[] = { { "/*", ROUTE_METHOD_ANY, any_any },
                                                            { "/*", ROUTE_METHOD_OPTIONS, options_cors },
                                                            { "/*", ROUTE_METHOD_HEAD, options_cors },

                                                            { "/", ROUTE_METHOD_GET, get_root },
                                                            { "/login", ROUTE_METHOD_GET, get_login },
                                                            { "/login", ROUTE_METHOD_POST, post_login },
                                                            { "/logout", ROUTE_METHOD_POST, post_logout },
                                                            { "/guilds", ROUTE_METHOD_GET, get_guilds },
                                                            { "/invite", ROUTE_METHOD_GET, get_invite },
                                                            { "/stream/:server_id", ROUTE_METHOD_GET, get_stream },
                                                            { NULL, ROUTE_METHOD_NULL, NULL } };

void
define_routes (APIApp *app)
{
    for (size_t i = 0; i < (sizeof (route_handlers) / sizeof (*route_handlers)); i++)
        {
            const route_handler_t *rh = &route_handlers[i];
            if (rh->method == ROUTE_METHOD_NULL)
                break;

            switch (rh->method)
                {
                case ROUTE_METHOD_GET:
                    if (!memcmp ("/stream/:server_id", rh->path, 19))
                        {
                            app->get (rh->path, rh->handler);
                            break;
                        }
                    app->get (rh->path, with_one_second_304 (rh));
                    break;

                case ROUTE_METHOD_POST:
                    app->post (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_OPTIONS:
                    app->options (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_HEAD:
                    app->head (rh->path, with_one_second_304 (rh));
                    break;

                case ROUTE_METHOD_ANY:
                    app->any (rh->path, rh->handler);
                    break;
                }
        }
}

} // musicat::server::routes
