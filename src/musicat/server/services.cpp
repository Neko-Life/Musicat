#include "musicat/server/services.h"
#include "musicat/musicat.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>

namespace musicat::server::services
{

curlpp_response_t
discord_post_creds (const std::string &creds)
{

    std::ostringstream os;

    curlpp::Easy req;

    req.setOpt (curlpp::options::Url (DISCORD_API_URL "/oauth2/token"));
    req.setOpt (curlpp::options::Header (1L));

    req.setOpt (curlpp::options::PostFields (creds));
    req.setOpt (curlpp::options::PostFieldSize (creds.length ()));

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

            return { false, 0, 0, "" };
        }

    return { true, curlpp::Infos::ResponseCode::get (req),
             curlpp::Infos::HeaderSize::get (req), os.str () };
}

curlpp_response_t
discord_get_me (const std::string &type, const std::string &token)
{
    std::ostringstream os;

    curlpp::Easy req;

    req.setOpt (curlpp::options::Url (DISCORD_API_URL "/oauth2/@me"));

    std::string header = "Authorization: " + type + ' ' + token;

    req.setOpt (curlpp::options::Header (1L));
    req.setOpt (curlpp::options::HttpHeader ({ header.c_str () }));

    req.setOpt (curlpp::options::Verbose (1L));

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

            return { false, 0, 0, "" };
        }

    return { true, curlpp::Infos::ResponseCode::get (req),
             curlpp::Infos::HeaderSize::get (req), os.str () };
}

} // musicat::server::services
