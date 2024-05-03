#include "musicat/server/auth.h"
#include "musicat/musicat.h"
#include "musicat/server/states.h"

namespace musicat::server::auth
{

#ifndef MUSICAT_NO_SERVER
std::string
create_jwt_token (const std::string &user_id)
{
    std::string secret = get_jwt_secret ();

    if (secret.empty ())
        {
            return "";
        }

    auto token = jwt::create ()
                     .set_issuer ("Musicat" /*version*/)
                     .set_type ("JWS")
                     .set_payload_claim ("user_id", jwt::claim (user_id))
                     .sign (jwt::algorithm::hs256{ secret });

    return std::string (token);
}

std::string
verify_jwt_token (const std::string &token)
{
    auto v = states::get_jwt_verifier_ptr ();

    if (!v || token.empty ())
        return "";

    std::string uid;

    try
        {
            auto decoded = jwt::decode (token);

            v->verify (decoded);

            picojson::value iss ("");

            for (auto &e : decoded.get_payload_json ())
                {
                    if (e.first == "iss")
                        iss = e.second;

                    if (e.first == "user_id")
                        uid = e.second.get<std::string> ();
                }

            if (iss.get<std::string> () != "Musicat" /*version*/)
                {
                    return "";
                }
        }
    catch (...)
        {
            return "";
        }

    return uid;
}
#endif

} // musicat::server::auth
