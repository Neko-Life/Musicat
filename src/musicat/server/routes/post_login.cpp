#include "musicat/db.h"
#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/auth.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/service_cache.h"
#include "musicat/server/states.h"
#include "musicat/thread_manager.h"

namespace musicat::server::routes
{

void
handle_post_login_creds (response::end_t &endres, const std::string &redirect,
                         const std::string &creds)
{
    services::curlpp_response_t resp = services::discord_post_creds (creds);

    /*
        oauth:
        {
          "token_type": string,
          "access_token": string,
          "expires_in": 604800,
          "refresh_token": string,
          "scope": string


        with guild:
          "guild": Guild object
        }
     */
    nlohmann::json udata = middlewares::process_curlpp_response_t (
        resp, "server::routes::handle_post_login_creds");

    if (resp.status != 200L)
        {
            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response
                = response::error (response::ERROR_CODE_NOTHING,
                                   std::string ("POST /oauth2/token: ")
                                       + std::to_string (resp.status))
                      .dump ();

            return;
        }

    if (!udata.is_object ())
        {
            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response
                = response::error (response::ERROR_CODE_NOTHING,
                                   "POST /oauth2/token: Unknown response")
                      .dump ();

            return;
        }

    ////////////////////////////////////////////////////////////////////////////////

    auto type = udata["token_type"];
    auto tkn = udata["access_token"];

    std::string token_type = type.is_string () ? type.get<std::string> () : "";
    std::string access_token = tkn.is_string () ? tkn.get<std::string> () : "";

    if (token_type.empty () || access_token.empty ())
        {
            fprintf (stderr,
                     "[server::routes::handle_post_login_body ERROR] POST "
                     "/oauth2/token Unknown response:\n%s\n",
                     udata.dump (2).c_str ());

            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response
                = response::error (response::ERROR_CODE_NOTHING,
                                   std::string ("POST /oauth2/token: ")
                                       + "Unknown response")
                      .dump ();

            return;
        }

    // get @me
    services::curlpp_response_t resp_me
        = services::discord_get_me (token_type, tkn);

    /*
        {
          "application": {
            "id": "886636156337414156",
            "name": "Musicat-alpha",
            "icon": null,
            "description": "",
            "type": null,
            "bot": {
              "id": "886636156337414156",
              "username": "Musicat-alpha",
              "avatar": null,
              "discriminator": "3640",
              "public_flags": 0,
              "premium_type": 0,
              "flags": 0,
              "bot": true,
              "banner": null,
              "accent_color": null,
              "global_name": null,
              "avatar_decoration_data": null,
              "banner_color": null
            },
            "summary": "",
            "bot_public": false,
            "bot_require_code_grant": false,
            "verify_key": string,
            "flags": 8945664,
            "hook": true,
            "is_monetized": false
          },
          "expires": "2023-11-05T16:34:02.853000+00:00",
          "scopes": [
            "guilds",
            "identify"
          ],
          "user": {
            "id": "750335181285490760",
            "username": "shasharina",
            "avatar": "854a22c8cd3b532d80ca2382f0c5e668",
            "discriminator": "0",
            "public_flags": 4194368,
            "premium_type": 2,
            "flags": 4194368,
            "banner": "641645c254a893beb35f1aecaa832509",
            "accent_color": 9437439,
            "global_name": "Shasha",
            "avatar_decoration_data": null,
            "banner_color": "#9000ff"
          }
        }
     */
    nlohmann::json ume = middlewares::process_curlpp_response_t (
        resp_me, "server::routes::handle_post_login_creds");

    if (resp_me.status != 200L)
        {
            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response
                = response::error (response::ERROR_CODE_NOTHING,
                                   std::string ("GET /oauth2/@me: ")
                                       + std::to_string (resp_me.status))
                      .dump ();

            return;
        }

    if (!ume.is_object ())
        {
            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response
                = response::error (response::ERROR_CODE_NOTHING,
                                   "GET /oauth2/@me: Unknown response")
                      .dump ();

            return;
        }

    auto i_user = ume["user"]["id"];

    std::string uid = !i_user.is_string () ? "" : i_user.get<std::string> ();

    if (uid.empty ())
        {
            fprintf (stderr,
                     "[server::routes::handle_post_login_body ERROR] GET "
                     "/oauth2/@me Unknown response:\n%s\n",
                     ume.dump (2).c_str ());

            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response
                = response::error (response::ERROR_CODE_NOTHING,
                                   std::string ("GET /oauth2/@me: ")
                                       + "Unknown response")
                      .dump ();

            return;
        }

    int64_t exp = 0;
    try
        {
            exp = udata.value ("expires_in", 0);
        }
    catch (...)
        {
            exp = 0;
        }

    if (exp < 0)
        exp = 0;

    std::string max_age = exp ? "; Max-Age=" + std::to_string (exp) : "";

    // save creds to db

    // remove guild property
    auto i_guild = udata.find ("guild");
    if (i_guild != udata.end ())
        {
            udata.erase (i_guild);
        }

    database::update_user_auth (uid, udata);
    service_cache::set_cached_user_auth (uid, udata);

    // set set-cookie header and end req

    const std::string token = auth::create_jwt_token (uid);
    if (token.empty ())
        {
            // ERROR
            fprintf (stderr,
                     "[server::routes::handle_post_login_body ERROR] Failed "
                     "generating token for %s\n",
                     uid.c_str ());

            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response = response::error (response::ERROR_CODE_NOTHING,
                                               "Failed generating token")
                                  .dump ();

            return;
        }

    endres.add_header ("Set-Cookie", std::string ("token=") + token + max_age
                                         + "; HttpOnly");

    endres.response = response::payload ({ { "redirect", redirect },
                                           { "user", ume["user"] } })
                          .dump ();
}

void
handle_post_login_body (const states::recv_body_t &recv)
{
    std::thread t ([recv] () {
        thread_manager::DoneSetter tmds;

        APIResponse *res = recv.res;
        APIRequest *req = recv.req;
        const std::string &body = recv.body;
        const auto &cors_headers = recv.cors_headers;

        response::end_t endres (res);
        response::defer_end_t defer_endres (endres);

        endres.status = http_status_t.BAD_REQUEST_400;
        endres.headers = cors_headers;
        endres.response = "body";

        // request might been aborted? or they actually sent empty body??
        // the former should never happen when it reaches here, so just reply
        // with bad request
        if (body.empty ())
            {
                return;
            }

        /*
           {
                "code": string,
                "state": string,
                "redirect_uri": string
           }
         */
        nlohmann::json json_body;

        try
            {
                json_body = nlohmann::json::parse (body);
            }
        catch (const nlohmann::json::exception &e)
            {
                fprintf (stderr,
                         "[server::routes::handle_post_login_body ERROR] %s\n",
                         e.what ());

                fprintf (stderr,
                         "================================================\n");
                std::cerr << body << '\n';
                fprintf (stderr,
                         "================================================\n");

                // let fall through so handled by below if
            }

        if (!json_body.is_object ())
            {
                return;
            }

        // the actual post handler starts here...

        auto i_state = json_body["state"];

        std::string state
            = !i_state.is_string () ? "" : i_state.get<std::string> ();

        if (state.empty ())
            {
                endres.response = "state";
                return;
            }

        // check if state valid and get previously saved redirect URL
        std::string redirect;
        int status = states::remove_oauth_state (state, &redirect);

        if (status)
            {
                // invalid state
                endres.status = http_status_t.FORBIDDEN_403;
                endres.response = "";
                return;
            }

        auto i_code = json_body["code"];

        std::string code
            = !i_code.is_string () ? "" : i_code.get<std::string> ();

        if (code.empty ())
            {
                endres.response = "code";
                return;
            }

        auto i_redirect_uri = json_body["redirect_uri"];

        std::string redirect_uri = !i_redirect_uri.is_string ()
                                       ? ""
                                       : i_redirect_uri.get<std::string> ();

        if (redirect_uri.empty ())
            {
                endres.response = "redirect_uri";
                return;
            }

        // we have the stuff here, proceed

        // verify to discord

        std::string secret = get_sha_secret ();

        if (secret.empty ())
            {
                // 😭😭😭
                fprintf (
                    stderr,
                    "[server::routes::post_login ERROR] No secret provided "
                    "in configuration file, can't process user login\n");

                endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
                endres.response = "config";

                return;
            }

        const std::string creds
            = "code=" + code + "&client_id=" + std::to_string (get_sha_id ())
              + "&client_secret=" + secret + "&grant_type="
              + "authorization_code" + "&redirect_uri=" + redirect_uri;

        endres.status = http_status_t.OK_200;
        endres.set_content_type_json ();
        endres.response = "";

        handle_post_login_creds (endres, redirect, creds);
    });

    thread_manager::dispatch (t);
}

void
post_login (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers (req); */

    states::recv_body_t struct_body = states::create_recv_body_t (
        "post_login", std::string (res->getRemoteAddressAsText ()), res, req,
        cors_headers);

    {
        std::lock_guard lk (states::recv_body_cache_m);
        int status = states::store_recv_body_cache (struct_body);

        if (status)
            {
                // this should never happen, there's smt wrong with your code
                fprintf (
                    stderr,
                    "[server::routes::post_login FATAL] Duplicate request "
                    "found, terminating\n");

                std::terminate ();
            }
    }

    res->onData (
        [cors_headers, struct_body] (std::string_view chunk, bool is_last) {
            std::lock_guard lk (states::recv_body_cache_m);
            std::vector<states::recv_body_t>::iterator cache
                = get_recv_body_cache (struct_body);

            if (is_recv_body_cache_end_iterator (cache))
                {
                    // request aborted, returns now
                    return;
                }

            cache->body.append (chunk);

            if (is_last)
                {
                    handle_post_login_body (*cache);

                    delete_recv_body_cache (cache);

                    return;
                }
        });

    res->onAborted ([struct_body] () {
        std::lock_guard lk (states::recv_body_cache_m);
        std::vector<states::recv_body_t>::iterator cache
            = get_recv_body_cache (struct_body);

        if (is_recv_body_cache_end_iterator (cache))
            {
                // well nothing to clean up, what else to do?
                return;
            }

        delete_recv_body_cache (cache);
    });

    /*
        auto redirect_uri_prop = d["redirect_uri"];
    remove_oauth_state
        if (!state_prop.is_string () || !redirect_uri_prop.is_string ()
            || code.empty ())
            {
                _set_resd_error (resd, "Invalid operation");
                break;
            }*/

    /*
    const std::string state = state_prop.get<std::string> ();

    if (_remove_oauth_state (state) < 0)
        {
            _set_resd_error (resd, "Unauthorized");
            break;
        }


    */
}

} // musicat::server::routes
