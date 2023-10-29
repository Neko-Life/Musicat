#ifndef MUSICAT_SERVER_ROUTES_GET_LOGIN_H
#define MUSICAT_SERVER_ROUTES_GET_LOGIN_H

#include "musicat/server.h"

namespace musicat::server::routes
{

void get_login (APIResponse *res, APIRequest *req);

} // musicat::server::routes

#endif // MUSICAT_SERVER_ROUTES_GET_LOGIN_H
