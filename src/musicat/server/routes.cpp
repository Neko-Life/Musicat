#include "musicat/server/routes.h"
#include "musicat/server.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"

// !TODO: maybe move this to smt like server_config.h
// a day
#define CORS_VALID_FOR "86400"

namespace musicat::server::routes
{

// any route
void
any_any (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    res->writeStatus (http_status_t.NOT_FOUND_404)->end ();
}

// cors protocol
void
options_cors (APIResponse *res, APIRequest *req)
{
    int status;
    status
        = middlewares::cors (res, req,
                             {
                                 { "Access-Control-Max-Age", CORS_VALID_FOR },
                             });

    if (status)
        return;

    res->writeStatus (http_status_t.NO_CONTENT_204)->end ();
}

void
get_root (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    middlewares::set_content_type_json (res);

    const char *http_status = http_status_t.OK_200;
    nlohmann::json r;

    auto bot = get_client_ptr ();
    if (!bot)
        {
            http_status = http_status_t.INTERNAL_SERVER_ERROR_500;
            r = response::error (response::ERROR_CODE_NOTHING,
                                 "Bot not running");
        }
    else
        r = response::payload (
            { { "avatar_url",
                bot->me.get_avatar_url (BOT_AVATAR_SIZE, dpp::i_webp) },
              { "username", bot->me.username },
              { "description", get_bot_description () } });

    res->writeStatus (http_status)->end (r.dump ());
}

void
get_login (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    middlewares::set_content_type_json (res);

    const char *http_status = http_status_t.OK_200;
    nlohmann::json r;

    std::string oauth_state = get_oauth_link ();
    if (!oauth_state.length ())
        {
            http_status = http_status_t.INTERNAL_SERVER_ERROR_500;
            r = response::error (response::ERROR_CODE_NOTHING,
                                 "No OAuth configured");
        }
    else
        r = response::payload (
            oauth_state + "&state=") /*!TODO: + _generate_oauth_state ()*/;

    res->writeStatus (http_status)->end (r.dump ());
}

void
post_login (APIResponse *res, APIRequest *req)
{
    int status;
    status = middlewares::cors (res, req);
    if (status)
        return;

    // set set-cookie header and end req

    /* auto state_prop = d["state"];
    auto redirect_uri_prop = d["redirect_uri"];
    const std::string code = d.value ("code", "");

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

    std::string secret = get_sha_secret ();

    if (secret.empty ())
        {
            _set_resd_error (resd, "No OAuth configured");
            break;
        }

    const std::string data
        = "code=" + code + "&client_id="
          + std::to_string (get_sha_id ()) + "&client_secret=" + secret
          + "&grant_type=" + "authorization_code"
          + "&redirect_uri=" + redirect_uri_prop.get<std::string> ();

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

inline constexpr const route_handler_t route_handlers[]
    = { { "/*", ROUTE_METHOD_ANY, any_any },
        { "/*", ROUTE_METHOD_OPTIONS, options_cors },

        { "/", ROUTE_METHOD_GET, get_root },
        { "/login", ROUTE_METHOD_GET, get_login },
        { "/login", ROUTE_METHOD_POST, post_login },
        { NULL, ROUTE_METHOD_NULL, NULL } };

void
define_routes (APIApp *app)
{
    for (size_t i = 0;
         i < (sizeof (route_handlers) / sizeof (*route_handlers)); i++)
        {
            const route_handler_t *rh = &route_handlers[i];
            if (rh->method == ROUTE_METHOD_NULL)
                break;

            switch (rh->method)
                {
                case ROUTE_METHOD_GET:
                    app->get (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_POST:
                    app->post (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_OPTIONS:
                    app->options (rh->path, rh->handler);
                    break;

                case ROUTE_METHOD_ANY:
                    app->any (rh->path, rh->handler);
                    break;
                }
        }
}

} // musicat::server::routes
