#include "musicat/server.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/states.h"

namespace musicat::server::routes
{

void
handle_post_login_body (APIResponse *res, APIRequest *req,
                        const std::string &body)
{
    // request might been aborted? or they actually sent empty body??
    // the former should never happen when it reaches here, so just reply with
    // bad request
    if (body.empty ())
        {
            res->writeStatus (http_status_t.BAD_REQUEST_400);
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
            res->end ("config");

            return;
        }

    const std::string data
        = "code=" + code + "&client_id=" + std::to_string (get_sha_id ())
          + "&client_secret=" + secret + "&grant_type=" + "authorization_code"
          + "&redirect_uri=" + redirect_uri;

    // spawn thread to verify so the main thread isn't blocked

    // !TODO

    // set set-cookie header and end req
}

void
post_login (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    states::recv_body_t struct_body = states::create_recv_body_t (
        "post_login", std::string (res->getRemoteAddressAsText ()));

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

    res->onData ([res, req, struct_body] (std::string_view chunk,
                                          bool is_last) mutable {
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
                        handle_post_login_body (res, req, std::string (chunk));
                        return;
                    }

                cache->body->append (chunk);

                handle_post_login_body (res, req, *cache->body);

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


    std::thread t (
        [] (const std::string creds) {
            thread_manager::DoneSetter tmds;
            std::ostringstream os;

            curlpp::Easy req;

            req.setOpt (curlpp::options::Url (DISCORD_API_URL
                                              "/oauth2/token"));

            req.setOpt (curlpp::options::Header (
                "Content-Type: "
                "application/x-www-form-urlencoded"));

            req.setOpt (curlpp::options::PostFields (creds));
            req.setOpt (
                curlpp::options::PostFieldSize (creds.length ()));

            req.setOpt (curlpp::options::WriteStream (&os));

            try
                {
                    req.perform ();
                }
            catch (const curlpp::LibcurlRuntimeError &e)
                {
                    fprintf (stderr,
                             "[ERROR] "
                             "LibcurlRuntimeError(%d): %s\n",
                             e.whatCode (), e.what ());

                    return;
                }

            // MAGIC INIT
            const std::string rawhttp = os.str ();

            fprintf (stderr, "%s\n", creds.c_str ());
            fprintf (stderr, "%s\n", rawhttp.c_str ());
        },
        data);

    thread_manager::dispatch (t);
    */
}

} // musicat::server::routes
