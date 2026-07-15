#ifndef MUSICAT_SERVER_ROUTES_POST_LOGOUT_H
#define MUSICAT_SERVER_ROUTES_POST_LOGOUT_H

#include "musicat/server.h"

namespace musicat::server::routes
{

void post_logout (APIResponse *res, APIRequest *req);

} // musicat::server::routes

#endif // MUSICAT_SERVER_ROUTES_POST_LOGOUT_H
