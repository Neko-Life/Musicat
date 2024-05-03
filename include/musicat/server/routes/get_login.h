#ifndef MUSICAT_SERVER_ROUTES_GET_LOGIN_H
#define MUSICAT_SERVER_ROUTES_GET_LOGIN_H

#include "musicat/server.h"

namespace musicat::server::routes
{

#ifndef MUSICAT_NO_SERVER
void get_login (APIResponse *res, APIRequest *req);
#endif

} // musicat::server::routes

#endif // MUSICAT_SERVER_ROUTES_GET_LOGIN_H
