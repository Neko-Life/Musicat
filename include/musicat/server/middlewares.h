#ifndef MUSICAT_SERVER_MIDDLEWARES_H
#define MUSICAT_SERVER_MIDDLEWARES_H

#include "musicat/server.h"
#include "musicat/server/services.h"
#include "nlohmann/json.hpp"

namespace musicat::server::middlewares
{
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

void set_guild_is_mutual (const std::string &user_id, nlohmann::json &guild);

} // musicat::server::middlewares

#endif // MUSICAT_SERVER_MIDDLEWARES_H
