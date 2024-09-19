#ifndef MUSICAT_SERVER_ROUTES_OAUTH_H
#define MUSICAT_SERVER_ROUTES_OAUTH_H

#include "musicat/server.h"

namespace musicat::server::routes::oauth
{

void get_oauth_handler (APIResponse *res, APIRequest *req,
                        std::string (*get_link) (void));

} // musicat::server::routes::oauth

#endif // MUSICAT_SERVER_ROUTES_OAUTH_H
