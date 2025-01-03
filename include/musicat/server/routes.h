#ifndef MUSICAT_SERVER_ROUTES_H
#define MUSICAT_SERVER_ROUTES_H

#include "musicat/server.h"

namespace musicat::server::routes
{

enum route_method_e
{
    ROUTE_METHOD_NULL,
    ROUTE_METHOD_GET,
    ROUTE_METHOD_POST,
    ROUTE_METHOD_OPTIONS,
    ROUTE_METHOD_HEAD,
    ROUTE_METHOD_ANY,
};

using route_handler_t_handler = void (*) (APIResponse *, APIRequest *);

struct route_handler_t
{
    const char *path;
    const route_method_e method;
    const route_handler_t_handler handler;
};

void define_routes (APIApp *app);

} // musicat::server::ws

#endif // MUSICAT_SERVER_ROUTES_H
