#include "musicat/server/middlewares.h"
#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/auth.h"
#include "musicat/server/services.h"

namespace musicat::server::middlewares
{

std::vector<std::string> _cors_enabled_origins;

header_v_t
get_cors_headers (std::string_view req_allow_headers)
{
    std::string allow_headers = "DNT,User-Agent,X-Requested-With,If-Modified-"
                                "Since,Cache-Control,Content-Type,Range";

    if (!req_allow_headers.empty ())
        {
            allow_headers += ',';
            allow_headers += req_allow_headers;
        }

    return {
        { "Access-Control-Allow-Methods", "GET, POST, OPTIONS" },
        { "Access-Control-Allow-Headers", allow_headers },
        { "Access-Control-Allow-Credentials", "true" },
        // other security headers
        { "X-Content-Type-Options", "nosniff" },
        { "X-XSS-Protection", "1; mode=block" },
    };
}

void
load_cors_enabled_origin ()
{
    _cors_enabled_origins = get_cors_enabled_origins ();
}

/*
 if ($request_method = 'OPTIONS') {
    add_header 'Access-Control-Allow-Origin' '*';
    add_header 'Access-Control-Allow-Methods' 'GET, POST, OPTIONS';
    #
    # Custom headers and headers various browsers *should* be OK with but
 aren't
    #
    add_header 'Access-Control-Allow-Headers'
 'DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,Content-Type,Range';




 // additional headers
    #
    # Tell client that this pre-flight info is valid for 20 days
    #
    add_header 'Access-Control-Max-Age' 1728000;
    add_header 'Content-Type' 'application/json';
    add_header 'Content-Length' 0;
    return 204;
 }
 if ($request_method = 'POST') {
 always; add_header 'Access-Control-Expose-Headers'
 'Content-Length,Content-Range' always;
 }
 if ($request_method = 'GET') {
 always; add_header 'Access-Control-Expose-Headers'
 'Content-Length,Content-Range' always;
 }
 */

header_v_t
cors (APIResponse *res, APIRequest *req, const header_v_t &additional_headers)
{
    std::string_view origin = req->getHeader ("origin");

    bool has_origin = !origin.empty ();

    if (!has_origin)
        {
            auto m = req->getMethod ();
            if (m != "get" && m != "head")
                {
                    res->writeStatus (http_status_t.BAD_REQUEST_400);
                    res->end ("Missing Origin header");

                    return {};
                }

            // origin = "*";
            // has_origin = true;
        }

    std::string_view host = req->getHeader ("host");

    bool has_host = !host.empty ();

    bool allow = has_origin
                     ? has_host ? (host.find (origin) == 0) : origin == "*"
                     : true;

    if (!allow)
        {
            for (const std::string &s : _cors_enabled_origins)
                {
                    if (origin == s)
                        {
                            allow = true;
                            break;
                        }
                }
        }

    if (!allow)
        {
            res->writeStatus (http_status_t.FORBIDDEN_403);
            res->end ("Disallowed Origin");
            return {};
        }

    header_v_t headers
        = { { "Access-Control-Allow-Origin", std::string (origin) } };

    std::string_view req_allow_headers
        = req->getHeader ("access-control-request-headers");

    for (const std::pair<std::string, std::string> &s :
         get_cors_headers (req_allow_headers))
        {
            headers.push_back (s);
        }

    for (const std::pair<std::string, std::string> &s : additional_headers)
        {
            headers.push_back (s);
        }

    return headers;
}

void
set_content_type_json (APIResponse *res)
{
    res->writeHeader ("Content-Type", "application/json");
}

void
write_headers (APIResponse *res, const header_v_t &headers)
{
    for (const std::pair<std::string, std::string> &s : headers)
        {
            res->writeHeader (s.first, s.second);
        }
}

void
print_headers (APIRequest *req)
{
    fprintf (stderr, "================================================\n");
    for (auto i = req->begin ().ptr; !i->key.empty (); i++)
        {
            std::cerr << i->key << ": " << i->value << '\n';
        }
    fprintf (stderr, "================================================\n");
}

inline constexpr const char token_cookie_name[] = "token=";
inline constexpr const size_t token_cookie_name_size
    = (sizeof (token_cookie_name) / sizeof (*token_cookie_name)) - 1;

std::string
validate_token (APIResponse *res, APIRequest *req,
                const header_v_t &cors_headers)
{
    std::string_view cookie = req->getHeader ("cookie");

    size_t i = cookie.find (token_cookie_name);

    if (i == cookie.npos)
        {
            res->writeStatus (http_status_t.UNAUTHORIZED_401);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return "";
        }

    size_t t_start = i + token_cookie_name_size;
    std::string_view token
        = cookie.substr (t_start, cookie.find (" ", t_start));

    if (token.empty ())
        {
            res->writeStatus (http_status_t.UNAUTHORIZED_401);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return "";
        }

    std::string user_id = auth::verify_jwt_token (std::string (token));

    if (user_id.empty ())
        {
            res->writeStatus (http_status_t.UNAUTHORIZED_401);
            middlewares::write_headers (res, cors_headers);
            res->end ();
            return "";
        }

    return user_id;
}

nlohmann::json
process_curlpp_response_t (const services::curlpp_response_t &resp,
                           const char *callee)
{
    if (resp.status != 200L)
        {
            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "[%s ERROR] Status: %ld\n", callee, resp.status);

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", resp.response.c_str ());

            fprintf (stderr,
                     "================================================\n");

            return nullptr;
        }

    std::string str_udata = resp.response.substr (resp.header_size).c_str ();

    nlohmann::json udata; // = { { "expires_in", 604800 } };

    try
        {
            udata = nlohmann::json::parse (str_udata);
        }
    catch (const nlohmann::json::exception &e)
        {
            fprintf (stderr, "[%s ERROR] %s\n", callee, e.what ());

            fprintf (stderr,
                     "================================================\n");

            fprintf (stderr, "%s\n", str_udata.c_str ());

            fprintf (stderr,
                     "================================================\n");
        }

    return udata;
}

} // musicat::server::middlewares
