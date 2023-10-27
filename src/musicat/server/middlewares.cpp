#include "musicat/musicat.h"
#include "musicat/server.h"

namespace musicat::server::middlewares
{

std::vector<std::string> _cors_enabled_origins;

std::vector<std::pair<std::string, std::string> > _cors_headers = {
    { "Access-Control-Allow-Methods", "GET, POST, OPTIONS" },
    { "Access-Control-Allow-Headers",
      "DNT,User-Agent,X-Requested-With,If-Modified-Since,Cache-Control,"
      "Content-Type,Range" },
};

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
cors (uWS::HttpResponse<SERVER_WITH_SSL> *res, uWS::HttpRequest *req,
      const std::vector<std::pair<std::string, std::string> >
          &additional_headers)
{
    std::string_view origin = req->getHeader ("origin");

    bool has_origin = !origin.empty ();

    if (!has_origin)
        {
            if (req->getMethod () != "GET" || req->getMethod () != "HEAD")
                {
                    res->writeStatus ("400 Bad Request")
                        ->end ("Missing Origin header");

                    return 1;
                }
        }

    std::string_view host = req->getHeader ("host");

    bool has_host = !host.empty ();

    bool allow
        = has_origin ? (has_host ? (host.find (origin) == 0) : false) : true;

    if (!allow)
        for (const std::string &s : _cors_enabled_origins)
            {
                if (origin == s)
                    {
                        allow = true;
                        break;
                    }
            }

    if (!allow)
        {
            res->writeStatus ("403 Forbidden")->end ("Origin");
            return 2;
        }

    res->writeHeader ("Access-Control-Allow-Origin", origin);

    for (const std::pair<std::string, std::string> &s : _cors_headers)
        {
            res->writeHeader (s.first, s.second);
        }

    for (const std::pair<std::string, std::string> &s : additional_headers)
        {
            res->writeHeader (s.first, s.second);
        }

    return 0;
}

} // musicat::server::middlewares
