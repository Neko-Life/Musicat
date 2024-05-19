#include "musicat/db.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/service_cache.h"
#include "musicat/thread_manager.h"

namespace musicat::server::routes
{

void
set_guilds_is_mutual (const std::string &user_id, nlohmann::json &guilds)
{
    if (!guilds.is_array ())
        return;

    for (nlohmann::json &g : guilds)
        {
            middlewares::set_guild_is_mutual (user_id, g);
        }
}

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

    response::end_t endres (res);
    endres.headers = cors_headers;

    // array of guild
    nlohmann::json cached = service_cache::get_cached_user_guilds (user_id);

    if (cached.is_array ())
        {
            // not included in cache and it's fine
            // some people just refresh to see if
            // the bot sucessfully invited to their
            // server
            set_guilds_is_mutual (user_id, cached);

            endres.set_content_type_json ();
            endres.response = response::payload (cached).dump ();

            return;
        }

    // required to be able to end req in another thread
    res->onAborted ([] () {
        // what to do on abort?
    });

    endres.res = NULL;

    const char *lstatus = endres.status;
    header_v_t lheaders = endres.headers;
    std::string lresponse = endres.response;

    std::thread t ([user_id, res, lstatus, lheaders, lresponse] () {
        thread_manager::DoneSetter tmds;

        response::end_t endres (res);
        endres.status = lstatus;
        endres.headers = lheaders;
        endres.response = lresponse;
        response::defer_end_t dendt (endres);

        nlohmann::json auth = service_cache::get_cached_user_auth (user_id);

        if (auth.is_null ())
            {
                auto db_auth = database::get_user_auth (user_id);
                auto auth_p = database::get_user_auth_json_from_PGresult (
                    db_auth.first);

                auth = auth_p.first;

                service_cache::set_cached_user_auth (user_id, auth);

                database::finish_res (db_auth.first);
                db_auth.first = nullptr;
            }

        if (!auth.is_object ())
            {
                endres.status = http_status_t.UNAUTHORIZED_401;
                endres.response = "Missing Auth";
                return;
            }

        auto type = auth["token_type"];
        auto tkn = auth["access_token"];

        std::string token_type
            = type.is_string () ? type.get<std::string> () : "";
        std::string access_token
            = tkn.is_string () ? tkn.get<std::string> () : "";

        if (token_type.empty () || access_token.empty ())
            {
                endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
                endres.response = "Invalid Credentials";

                return;
            }

        services::curlpp_response_t resp
            = services::discord_get_user_guilds (token_type, access_token);

        // array of guild
        nlohmann::json r = middlewares::process_curlpp_response_t (
            resp, "server::routes::get_guilds");

        if (resp.status != 200L)
            {
                endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
                endres.set_content_type_json ();
                endres.response
                    = response::error (response::ERROR_CODE_NOTHING,
                                       std::string ("GET /users/@me/guilds: ")
                                           + std::to_string (resp.status))
                          .dump ();

                return;
            }

        service_cache::set_cached_user_guilds (user_id, r);

        set_guilds_is_mutual (user_id, r);

        endres.set_content_type_json ();
        endres.response = response::payload (r).dump ();
    });

    thread_manager::dispatch (t);
}

} // musicat::server::routes
