#ifndef MUSICAT_SERVER_ROUTES_H
#define MUSICAT_SERVER_ROUTES_H

#include "musicat/server.h"

namespace musicat::server::routes
{

#ifndef MUSICAT_NO_SERVER
enum route_method_e
{
    ROUTE_METHOD_NULL,
    ROUTE_METHOD_GET,
    ROUTE_METHOD_POST,
    ROUTE_METHOD_OPTIONS,
    ROUTE_METHOD_ANY,
};

struct route_handler_t
{
    const char *path;
    const route_method_e method;
    void (*const handler) (APIResponse *, APIRequest *);
};

void define_routes (APIApp *app);
#endif

} // musicat::server::ws

#endif // MUSICAT_SERVER_ROUTES_H
