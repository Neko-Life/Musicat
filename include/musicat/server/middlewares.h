#ifndef MUSICAT_SERVER_MIDDLEWARES_H
#define MUSICAT_SERVER_MIDDLEWARES_H

#include "musicat/musicat.h"
#include "musicat/server.h"

namespace musicat::server::middlewares
{

void load_cors_enabled_origin ();

std::vector<std::pair<std::string, std::string> > cors (
    APIResponse *res, APIRequest *req,
    const std::vector<std::pair<std::string, std::string> > &additional_headers
    = { { "Access-Control-Expose-Headers", "Content-Length,Content-Range" } });

void set_content_type_json (APIResponse *res);

void write_headers (
    APIResponse *res,
    const std::vector<std::pair<std::string, std::string> > &headers);

void print_headers (APIRequest *req);

} // musicat::server::middlewares

#endif // MUSICAT_SERVER_MIDDLEWARES_H
