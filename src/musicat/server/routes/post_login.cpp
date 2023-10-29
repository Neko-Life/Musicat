#include "musicat/server.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/services.h"
#include "musicat/server/states.h"
#include "musicat/thread_manager.h"

namespace musicat::server::routes
{

void
handle_post_login_creds (
    APIResponse *res, std::string redirect,
    std::vector<std::pair<std::string, std::string> > cors_headers,
    std::string creds)
{
    thread_manager::DoneSetter tmds;

    services::curlpp_response_t resp = services::discord_post_creds (creds);

    if (resp.status != 200L)
        {
            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr,
                     "[server::routes::handle_post_login_body ERROR] POST "
                     "/oauth2/token Status: %ld\n",
                     resp.status);

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", creds.c_str ());

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", resp.response.c_str ());

            fprintf (stderr,
                     "================================================\n");

            res->writeStatus (http_status_t.INTERNAL_SERVER_ERROR_500);
            middlewares::write_headers (res, cors_headers);
            res->end (response::error (response::ERROR_CODE_NOTHING,
                                       std::string ("POST /oauth2/token: ")
                                           + std::to_string (resp.status))
                          .dump ());

            return;
        }

    std::string str_udata = resp.response.substr (resp.header_size).c_str ();

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
    nlohmann::json udata;

    try
        {
            udata = nlohmann::json::parse (str_udata);
        }
    catch (const nlohmann::json::exception &e)
        {
            fprintf (stderr,
                     "[server::routes::handle_post_login_body ERROR] %s\n",
                     e.what ());

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", str_udata.c_str ());

            fprintf (stderr,
                     "================================================\n");
        }

    if (!udata.is_object ())
        {
            res->writeStatus (http_status_t.INTERNAL_SERVER_ERROR_500);
            middlewares::write_headers (res, cors_headers);
            res->end (response::error (response::ERROR_CODE_NOTHING,
                                       "POST /oauth2/token: Unknown response")
                          .dump ());

            return;
        }

    ////////////////////////////////////////////////////////////////////////////////

    // get @me
    services::curlpp_response_t resp_me = services::discord_get_me (
        udata.find ("token_type")->get<std::string> (),
        udata.find ("access_token")->get<std::string> ());

    if (resp_me.status != 200L)
        {
            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr,
                     "[server::routes::handle_post_login_body ERROR] GET "
                     "/oauth2/@me Status: %ld\n",
                     resp_me.status);

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", udata.dump (2).c_str ());

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", resp_me.response.c_str ());

            fprintf (stderr,
                     "================================================\n");

            res->writeStatus (http_status_t.INTERNAL_SERVER_ERROR_500);
            middlewares::write_headers (res, cors_headers);
            res->end (response::error (response::ERROR_CODE_NOTHING,
                                       std::string ("GET /oauth2/@me: ")
                                           + std::to_string (resp_me.status))
                          .dump ());

            return;
        }

    fprintf (stderr, "get @me:\n%s\n", resp_me.response.c_str ());

    std::string str_ume
        = resp_me.response.substr (resp_me.header_size).c_str ();

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
    nlohmann::json ume;

    try
        {
            ume = nlohmann::json::parse (str_ume);
        }
    catch (const nlohmann::json::exception &e)
        {
            fprintf (stderr,
                     "[server::routes::handle_post_login_body ERROR] %s\n",
                     e.what ());

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", str_ume.c_str ());

            fprintf (stderr,
                     "================================================\n");
        }

    if (!ume.is_object ())
        {
            res->writeStatus (http_status_t.INTERNAL_SERVER_ERROR_500);
            middlewares::write_headers (res, cors_headers);
            res->end (response::error (response::ERROR_CODE_NOTHING,
                                       "GET /oauth2/@me: Unknown response")
                          .dump ());

            return;
        }

    // save creds to db
    // !TODO

    // set set-cookie header and end req
    // !TODO

    res->writeStatus (http_status_t.OK_200);
    middlewares::write_headers (res, cors_headers);
    res->end (response::payload ({ "redirect", redirect }).dump ());
}

void
handle_post_login_body (
    APIResponse *res, APIRequest *req, const std::string &body,
    const std::vector<std::pair<std::string, std::string> > &cors_headers)
{
    // request might been aborted? or they actually sent empty body??
    // the former should never happen when it reaches here, so just reply with
    // bad request
    if (body.empty ())
        {
            res->writeStatus (http_status_t.BAD_REQUEST_400);
            middlewares::write_headers (res, cors_headers);
            res->end ("body");
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

            // let fall through so handled by below if
        }

    if (!json_body.is_object ())
        {
            res->writeStatus (http_status_t.BAD_REQUEST_400);
            middlewares::write_headers (res, cors_headers);
            res->end ("body");
            return;
        }

    // the actual post handler starts here...

    auto i_state = json_body.find ("state");

    bool i_state_is_end = i_state == json_body.end ();

    std::string state = (i_state_is_end || !i_state->is_string ())
                            ? ""
                            : i_state->get<std::string> ();

    if (state.empty ())
        {
            res->writeStatus (http_status_t.BAD_REQUEST_400);
            middlewares::write_headers (res, cors_headers);
            res->end ("state");
            return;
        }

    auto i_code = json_body.find ("code");

    bool i_code_is_end = i_code == json_body.end ();

    std::string code = (i_code_is_end || !i_code->is_string ())
                           ? ""
                           : i_code->get<std::string> ();

    if (code.empty ())
        {
            res->writeStatus (http_status_t.BAD_REQUEST_400);
            middlewares::write_headers (res, cors_headers);
            res->end ("code");
            return;
        }

    auto i_redirect_uri = json_body.find ("redirect_uri");

    bool i_redirect_uri_is_end = i_redirect_uri == json_body.end ();

    std::string redirect_uri
        = (i_redirect_uri_is_end || !i_redirect_uri->is_string ())
              ? ""
              : i_redirect_uri->get<std::string> ();

    if (redirect_uri.empty ())
        {
            res->writeStatus (http_status_t.BAD_REQUEST_400);
            middlewares::write_headers (res, cors_headers);
            res->end ("redirect_uri");
            return;
        }

    // we have the stuff here, proceed

    // check if state valid and get previously saved redirect URL
    std::string redirect;
    int status = states::remove_oauth_state (state, &redirect);

    if (status)
        {
            // invalid state
            res->writeStatus (http_status_t.FORBIDDEN_403);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return;
        }

    // verify to discord

    std::string secret = get_sha_secret ();

    if (secret.empty ())
        {
            // 😭😭😭
            fprintf (stderr,
                     "[server::routes::post_login ERROR] No secret provided "
                     "in configuration file, can't process user login\n");

            res->writeStatus (http_status_t.INTERNAL_SERVER_ERROR_500);
            middlewares::write_headers (res, cors_headers);
            res->end ("config");

            return;
        }

    std::string creds
        = "code=" + code + "&client_id=" + std::to_string (get_sha_id ())
          + "&client_secret=" + secret + "&grant_type=" + "authorization_code"
          + "&redirect_uri=" + redirect_uri;

    // spawn thread to verify so the main thread isn't blocked

    std::thread t (handle_post_login_creds, res, redirect, cors_headers,
                   creds);

    thread_manager::dispatch (t);
}

void
post_login (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    states::recv_body_t struct_body = states::create_recv_body_t (
        "post_login", std::string (res->getRemoteAddressAsText ()), res, req);

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

            if (is_last)
                {
                    if (!cache->body)
                        {
                            handle_post_login_body (cache->res, cache->req,
                                                    std::string (chunk),
                                                    cors_headers);

                            return;
                        }

                    cache->body->append (chunk);

                    handle_post_login_body (cache->res, cache->req,
                                            *cache->body, cors_headers);

                    delete_recv_body_cache (cache);

                    return;
                }

            if (!cache->body)
                cache->body = new std::string ();

            cache->body->append (chunk);
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
