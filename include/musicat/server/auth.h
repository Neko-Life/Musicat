#ifndef MUSICAT_SERVER_AUTH_H
#define MUSICAT_SERVER_AUTH_H

#include "jwt-cpp/jwt.h"

namespace musicat::server::auth
{
using jwt_verifier_t
    = jwt::verifier<jwt::default_clock, jwt::traits::kazuho_picojson>;

std::string create_jwt_token (const std::string &user_id);

std::string verify_jwt_token (const std::string &token);

} // musicat::server::auth

#endif // MUSICAT_SERVER_AUTH_H
