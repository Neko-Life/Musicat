#include "musicat/cmds/owner.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include "musicat/util/base64.h"
#include <curlpp/Infos.hpp>

inline constexpr size_t max_avatar_upload_size = 10240 * 1000;
#define MAX_IMG_SIZE max_avatar_upload_size

namespace musicat::command::owner::set_avatar
{

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (dpp::co_sub_command, "set_avatar",
                                "Set Musicat avatar");

    subcmd.add_option (dpp::command_option (dpp::co_attachment, "image",
                                            "Image[ to set as avatar]", true));

    slash.add_option (subcmd);
}

std::vector<std::string> valid_avatar_formats
    = { "image/jpeg", "image/png", "image/gif" };

void
slash_run (const dpp::slashcommand_t &event)
{
    std::string musicat_username = (event.from && event.from->creator)
                                       ? event.from->creator->me.username
                                       : "";

    if (musicat_username.empty ())
        return event.reply (
            "`[ERROR]` For some reason I can't remember my name, why do I "
            "need my name for this in the first place, ugh...");

    dpp::snowflake img_id = 0;
    get_inter_param (event, "image", &img_id);

    if (!img_id)
        return event.reply ("Seems like you didn't provide any image?");

    event.thinking ();

    std::thread rt ([event, img_id, musicat_username] () {
        thread_manager::DoneSetter tmds;

        dpp::attachment att = event.command.get_resolved_attachment (img_id);

        bool valid_type = false;

        for (const auto &type : valid_avatar_formats)
            {
                if (type == att.content_type)
                    {
                        valid_type = true;
                        break;
                    }
            }

        if (!valid_type)
            {
                return event.edit_response (
                    "Valid avatar image format are only JPEG, PNG and GIF");
            }

        std::ostringstream os;
        curlpp::Easy req;
        req.setOpt (curlpp::options::Url (att.url));

        req.setOpt (curlpp::options::Header (1L));
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

                event.edit_response ("`[ERROR]` Unable to fetch attachment");

                return;
            }

        std::string res_str = os.str ();

        long hsiz = curlpp::Infos::HeaderSize::get (req);

        std::string header_str = res_str.substr (0, hsiz);
        std::string res_body = res_str.substr (hsiz);

        if (res_str.empty () || res_body.empty ())
            {
                std::cerr << "[command::owner::set_avatar ERROR] Header:\n"
                          << header_str << "\n\n"
                          << att.content_type << '\n';

                return event.edit_response (
                    "Something went wrong when fetching attachment...");
            }

        std::string base64_data = util::base64::encode_standard (res_body);
        size_t img_len = base64_data.length ();

        bool debug = get_debug_state ();

        if (debug)
            {
                std::cerr << "[command::owner::set_avatar] Image data:\n"
                          << header_str << "\n\nImage length: " << img_len
                          << "\nContent Type: " << att.content_type << '\n';
            }

        if (img_len > MAX_IMG_SIZE)
            {
                return event.edit_response (
                    "File cannot be larger than 10240.0 kb.");
            }

        os.str ("");
        os.clear ();
        req.reset ();

        req.setOpt (curlpp::options::Url (DISCORD_API_URL "/users/@me"));

        std::string auth_header = "Authorization: Bot " + get_sha_token ();
        std::string header = "Content-Type: application/json";
        req.setOpt (curlpp::options::HttpHeader ({
            auth_header.c_str (),
            header.c_str (),
        }));

        // req.setOpt (curlpp::options::Verbose (1L));

        req.setOpt (curlpp::options::Header (1L));
        req.setOpt (curlpp::options::WriteStream (&os));

        std::string body
            = nlohmann::json ({ { "username", musicat_username },
                                { "avatar",
                                  // data:
                                  "data:"
                                      // image/jpeg
                                      + att.content_type
                                      // ;base64,
                                      + ";base64,"
                                      // BASE64_ENCODED_JPEG_IMAGE_DATA
                                      + base64_data } })
                  .dump ();

        req.setOpt (curlpp::options::CustomRequest{ "PATCH" });
        req.setOpt (curlpp::options::PostFields (body));
        req.setOpt (curlpp::options::PostFieldSize (body.length ()));

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

                event.edit_response ("`[ERROR]` Failed to upload attachment");

                return;
            }

        long s_code = curlpp::Infos::ResponseCode::get (req);
        bool patch_err = s_code != 200;

        if (patch_err || debug)
            {
                std::cerr << "PATCH AVATAR:\n" << os.str () << '\n';
            }

        if (patch_err)
            {
                return event.edit_response ("`[ERROR]` Status "
                                            + std::to_string (s_code));
            }

        event.edit_response ("Done!");
    });

    thread_manager::dispatch (rt);
}

} // musicat::command::owner::set_avatar
