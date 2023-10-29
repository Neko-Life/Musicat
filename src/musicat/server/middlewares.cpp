#include "musicat/musicat.h"
#include "musicat/server.h"

namespace musicat::server::middlewares
{

std::vector<std::string> _cors_enabled_origins;

std::vector<std::pair<std::string, std::string> >
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

int
cors (APIResponse *res, APIRequest *req,
      const std::vector<std::pair<std::string, std::string> >
          &additional_headers)
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

                    return 1;
                }

            // origin = "*";
            // has_origin = true;
        }

    std::string_view host = req->getHeader ("host");

    bool has_host = !host.empty ();

    bool allow = has_origin ? has_host        ? (host.find (origin) == 0)
                              : origin == "*" ? true
                                              : false
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
            res->end ("Origin");
            return 2;
        }

    res->writeHeader ("Access-Control-Allow-Origin", origin);

    std::string_view req_allow_headers
        = req->getHeader ("access-control-request-headers");

    for (const std::pair<std::string, std::string> &s :
         get_cors_headers (req_allow_headers))
        {
            res->writeHeader (s.first, s.second);
        }

    for (const std::pair<std::string, std::string> &s : additional_headers)
        {
            res->writeHeader (s.first, s.second);
        }

    return 0;
}

void
set_content_type_json (APIResponse *res)
{
    res->writeHeader ("Content-Type", "application/json");
}

} // musicat::server::middlewares