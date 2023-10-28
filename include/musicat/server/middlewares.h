#ifndef MUSICAT_SERVER_MIDDLEWARES_H
#define MUSICAT_SERVER_MIDDLEWARES_H

#include "musicat/musicat.h"
#include "musicat/server.h"

namespace musicat::server::middlewares
{

void load_cors_enabled_origin ();

int cors (
    APIResponse *res, APIRequest *req,
    const std::vector<std::pair<std::string, std::string> > &additional_headers
    = { { "Access-Control-Expose-Headers", "Content-Length,Content-Range" } });

} // musicat::server::middlewares

#endif // MUSICAT_SERVER_MIDDLEWARES_H
