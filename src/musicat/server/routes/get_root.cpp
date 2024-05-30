#include "musicat/musicat.h"
#include "musicat/server/middlewares.h"
#include "musicat/server/response.h"
#include "musicat/server/service_cache.h"

namespace musicat::server::routes
{

void
set_endres_response_with_musicat_data (dpp::cluster *bot,
                                       response::end_t &endres,
                                       const nlohmann::json &musicat_data)
{
    const bool has_detailed_data = musicat_data.is_object ();

    endres.response
        = response::payload (
              {
                  { "avatar_url",
                    bot->me.get_avatar_url (BOT_AVATAR_SIZE, dpp::i_webp) },
                  { "username", bot->me.username },
                  { "description", get_bot_description () },
                  {
                      "banner_url",
                      has_detailed_data ? musicat_data["banner_url"] : nullptr,
                  },
              })
              .dump ();
}

void
get_root (APIResponse *res, APIRequest *req)
{
    auto cors_headers = middlewares::cors (res, req);
    if (cors_headers.empty ())
        return;

    /* middlewares::print_headers (req); */

    response::end_t endres (res);
    endres.headers = cors_headers;
    endres.set_content_type_json ();

    auto bot = get_client_ptr ();
    if (!bot)
        {
            endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
            endres.response = response::error (response::ERROR_CODE_NOTHING,
                                               "Bot not running")
                                  .dump ();

            return;
        }

    const nlohmann::json musicat_data
        = service_cache::get_cached_musicat_detailed_user ();
    if (musicat_data.is_object ())
        {
            set_endres_response_with_musicat_data (bot, endres, musicat_data);

            return;
        }

    // required to be able to end req in another thread
    res->onAborted ([] () {
        // what to do on abort?
    });

    endres.res = NULL;

    const char *lstatus = endres.status;
    header_v_t lheaders = endres.headers;
    std::string lresponse = endres.response;
    bot->current_user_get ([res, lstatus, lheaders, lresponse] (
                               const dpp::confirmation_callback_t &ev) {
        response::end_t endres (res);
        endres.status = lstatus;
        endres.headers = lheaders;
        endres.response = lresponse;
        response::defer_end_t dendt (endres);

        auto bot = get_client_ptr ();
        if (!bot) [[unlikely]]
            {
                endres.status = http_status_t.INTERNAL_SERVER_ERROR_500;
                endres.response
                    = response::error (response::ERROR_CODE_NOTHING,
                                       "Bot not running")
                          .dump ();

                return;
            }

        nlohmann::json musicat_data;

        if (ev.is_error ())
            {
                fprintf (stderr,
                         "[server::routes::get_root bot->current_user_get "
                         "ERROR]:\n%s\n",
                         ev.http_info.body.c_str ());
            }
        else
            {
                auto musicat_u = ev.get<dpp::user_identified> ();
                const bool musicat_u_has_banner
                    = (musicat_u.banner.first != 0
                       && musicat_u.banner.second != 0);

                musicat_data["locale"] = musicat_u.locale;
                musicat_data["email"] = musicat_u.email;
                musicat_data["accent_color"] = musicat_u.accent_color;
                musicat_data["verified"] = musicat_u.verified;
                musicat_data["banner"] = musicat_u_has_banner
                                             ? musicat_u.banner.to_string ()
                                             : nullptr;
                musicat_data["banner_url"]
                    = musicat_u_has_banner
                          ? musicat_u.get_banner_url (4096, dpp::i_webp)
                          : nullptr;
            }

        if (musicat_data.is_object ())
            {
                service_cache::set_cached_musicat_detailed_user (musicat_data);
            }

        set_endres_response_with_musicat_data (bot, endres, musicat_data);
    });
}

} // musicat::server::routes
