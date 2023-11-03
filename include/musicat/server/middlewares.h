#ifndef MUSICAT_SERVER_MIDDLEWARES_H
#define MUSICAT_SERVER_MIDDLEWARES_H

#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/services.h"

namespace musicat::server::middlewares
{
using header_v_t = std::vector<std::pair<std::string, std::string> >;

void load_cors_enabled_origin ();

header_v_t cors (APIResponse *res, APIRequest *req,
                 const header_v_t &additional_headers
                 = { { "Access-Control-Expose-Headers",
                       "Content-Length,Content-Range" } });

void set_content_type_json (APIResponse *res);

void write_headers (APIResponse *res, const header_v_t &headers);

void print_headers (APIRequest *req);

std::string validate_token (APIResponse *res, APIRequest *req,
                            const header_v_t &cors_headers);

nlohmann::json
process_curlpp_response_t (const services::curlpp_response_t &resp,
                           const char *callee);

} // musicat::server::middlewares

#endif // MUSICAT_SERVER_MIDDLEWARES_H
