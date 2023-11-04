#ifndef MUSICAT_SERVER_ROUTES_GET_GUILDS_H
#define MUSICAT_SERVER_ROUTES_GET_GUILDS_H

#include "musicat/server.h"

namespace musicat::server::routes
{

void get_guilds (APIResponse *res, APIRequest *req);

} // musicat::server::routes

#endif // MUSICAT_SERVER_ROUTES_GET_GUILDS_H
