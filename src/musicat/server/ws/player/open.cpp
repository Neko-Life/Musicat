#include "musicat/musicat.h"
#include "musicat/server/ws/player.h"
#ifndef MUSICAT_NO_SERVER
#include <uWebSockets/src/App.h>
#endif

namespace musicat::server::ws::player::events
{
#ifndef MUSICAT_NO_SERVER

nlohmann::json
get_bot_info_payload (dpp::cluster *bot)
{
    bool debug = get_debug_state ();

    nlohmann::json d
        = { { "avatarUrl",
              bot->me.get_avatar_url (BOT_AVATAR_SIZE, dpp::i_webp) },
            { "username", bot->me.username },
            { "description", get_bot_description () } };

    if (debug)
        fprintf (stderr, "[server get_bot_info_payload]:\n%s\n",
                 d.dump (2).c_str ());

    return d;
}

void
open (uws_ws_t *ws)
{
    const bool debug = get_debug_state ();

    if (debug)
        {
            fprintf (stderr, "[server OPEN] %lu\n", (uintptr_t)ws);
        }

    ws->subscribe ("bot_info_update");

    auto bot = get_client_ptr ();

    if (!bot)
        {
            return;
        }

    nlohmann::json payload_bot_info = get_bot_info_payload (bot);
    ws->send (payload_bot_info.dump ());

    /*
                        //!TODO: this should get user guilds instead
                    case ws_req_t::server_list:
                        {
                            // on open
                            auto *guild_cache = dpp::get_guild_cache ();
                            if (!guild_cache)
                                {
                                    _set_resd_error (resd, "No guild cached");
                                    break;
                                }

                            // lock cache mutex for thread safety
                            std::shared_mutex &cache_mutex
                                = guild_cache->get_mutex ();
                            std::lock_guard<std::shared_mutex &> lk
       (cache_mutex);

                            for (auto &pair : guild_cache->get_container ())
                                {
                                    dpp::guild *guild = pair.second;
                                    if (!guild)
                                        continue;

                                    nlohmann::json to_push;

                                    try
                                        {
                                            to_push = nlohmann::json::parse (
                                                guild->build_json (true));
                                        }
                                    catch (...)
                                        {
                                            fprintf (
                                                stderr,
                                                "[server::_handle_req ERROR] "
                                                "Error building guild json\n");
                                            continue;
                                        }

                                    to_push["icon_url"]
                                        = guild->get_icon_url (512,
       dpp::i_webp); to_push["banner_url"] = guild->get_banner_url ( 1024,
       dpp::i_webp); to_push["splash_url"] = guild->get_splash_url ( 1024,
       dpp::i_webp); to_push["discovery_splash_url"] =
       guild->get_discovery_splash_url ( 1024, dpp::i_webp);

                                    resd.push_back (to_push);
                                }

                            break;
                        }
    */
}

#endif
} // musicat::server::ws::player::events
