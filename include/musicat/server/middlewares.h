#ifndef MUSICAT_SERVER_MIDDLEWARES_H
#define MUSICAT_SERVER_MIDDLEWARES_H

#include "musicat/musicat.h"
#include "musicat/server.h"

namespace musicat::server::middlewares
{

void load_cors_enabled_origin ();

int cors (
    uWS::HttpResponse<SERVER_WITH_SSL> *res, uWS::HttpRequest *req,
    const std::vector<std::pair<std::string, std::string> > &additional_headers
    = { { "Access-Control-Expose-Headers", "Content-Length,Content-Range" } });

} // musicat::server::middlewares

#endif // MUSICAT_SERVER_MIDDLEWARES_H
