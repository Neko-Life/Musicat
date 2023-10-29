#ifndef MUSICAT_SERVER_SERVICES_H
#define MUSICAT_SERVER_SERVICES_H

#include <string>

namespace musicat::server::services
{

struct curlpp_response_t
{
    bool success;
    long status;
    long header_size;
    std::string response;
};

curlpp_response_t discord_post_creds (const std::string &creds);

curlpp_response_t discord_get_me (const std::string &type,
                                  const std::string &token);

} // musicat::server::services

#endif // MUSICAT_SERVER_SERVICES_H
