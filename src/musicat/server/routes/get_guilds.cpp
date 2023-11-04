#include "musicat/db.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/service_cache.h"

namespace musicat::server::routes
{

void
get_guilds (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers (req); */

    std::string user_id = middlewares::validate_token (res, req, cors_headers);
    if (user_id.empty ())
        return;

    const char *http_status = http_status_t.OK_200;

    // array of guild
    nlohmann::json cached = service_cache::get_cached_user_guilds (user_id);

    if (!cached.is_null ())
        {
            res->writeStatus (http_status);
            middlewares::write_headers (res, cors_headers);
            middlewares::set_content_type_json (res);
            res->end (cached.dump ());

            return;
        }

    nlohmann::json auth = service_cache::get_cached_user_auth (user_id);

    if (auth.is_null ())
        {
            auto db_auth = database::get_user_auth (user_id);
            auto auth_p
                = database::get_user_auth_json_from_PGresult (db_auth.first);

            auth = auth_p.first;

            service_cache::set_cached_user_auth (user_id, auth);

            database::finish_res (db_auth.first);
            db_auth.first = nullptr;
        }

    if (!auth.is_object ())
        {
            res->writeStatus (http_status_t.UNAUTHORIZED_401);
            middlewares::write_headers (res, cors_headers);
            res->end ("Missing Auth");
            return;
        }

    auto type = auth["token_type"];
    auto tkn = auth["access_token"];

    std::string token_type = type.is_string () ? type.get<std::string> () : "";
    std::string access_token = tkn.is_string () ? tkn.get<std::string> () : "";

    if (token_type.empty () || access_token.empty ())
        {
            res->writeStatus (http_status_t.INTERNAL_SERVER_ERROR_500);
            middlewares::write_headers (res, cors_headers);
            res->end ("Invalid Credentials");

            return;
        }

    services::curlpp_response_t resp
        = services::discord_get_user_guilds (token_type, access_token);

    // array of guild
    nlohmann::json r = middlewares::process_curlpp_response_t (
        resp, "server::routes::get_guilds");

    service_cache::set_cached_user_guilds (user_id, r);

    res->writeStatus (http_status);
    middlewares::write_headers (res, cors_headers);
    middlewares::set_content_type_json (res);
    res->end (r.dump ());
}

} // musicat::server::routes
