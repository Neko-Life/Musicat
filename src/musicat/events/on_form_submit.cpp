#include "musicat/events/on_form_submit.h"
#include "musicat/cmds/play.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/storage.h"
#include "musicat/thread_manager.h"
#include "musicat/util_response.h"
#include "yt-search/yt-search.h"
#include <regex>

namespace musicat::events
{
void
_handle_modal_p_que_s_track (const dpp::form_submit_t &event,
                             const dpp::component &comp,
                             dpp::component &comp_2, bool &second_input)
{
    int64_t pos = 0;
    {
        if (!std::holds_alternative<std::string> (comp.value))
            return;

        std::string q = std::get<std::string> (comp.value);
        if (q.empty ())
            return;

        sscanf (q.c_str (), "%ld", &pos);
        if (pos < 1)
            return;
    }

    bool top = comp.custom_id.find ("top") != std::string::npos;

    int64_t arg_slip = 0;
    if (second_input)
        {
            if (!std::holds_alternative<std::string> (comp_2.value))
                return;

            std::string q = std::get<std::string> (comp_2.value);
            if (q.empty ())
                return;

            sscanf (q.c_str (), "%ld", &arg_slip);

            if (arg_slip < 1)
                return;

            if (arg_slip == 1)
                top = true;
        }

    auto storage = storage::get (event.command.message_id);
    if (!storage.has_value ())
        {
            dpp::message m ("It seems like this result is "
                            "outdated, try make a new search");

            m.flags |= dpp::m_ephemeral;
            event.reply (m);
            return;
        }

    auto tracks = std::any_cast<std::vector<yt_search::YTrack> > (storage);
    if (tracks.size () < (size_t)pos)
        return;

    auto result = tracks.at (pos - 1);

    auto from = event.from;
    auto guild_id = event.command.guild_id;

    const std::string prepend_name
        = util::response::str_mention_user (event.command.usr.id);

    std::string edit_response
        = prepend_name
          + util::response::reply_added_track (mctrack::get_title (result),
                                               top ? 1 : arg_slip);

    event.thinking ();

    std::string fname = std::regex_replace (
        mctrack::get_title (result) + std::string ("-") + result.id ()
            + std::string (".opus"),
        std::regex ("/"), "", std::regex_constants::match_any);

    bool dling = false;

    std::ifstream test (get_music_folder_path () + fname,
                        std::ios_base::in | std::ios_base::binary);

    if (!test.is_open ())
        {
            dling = true;
            event.edit_response (prepend_name
                                 + util::response::reply_downloading_track (
                                     mctrack::get_title (result)));

            auto player_manager = get_player_manager_ptr ();

            if (player_manager
                && player_manager->waiting_file_download.find (fname)
                       == player_manager->waiting_file_download.end ())
                {
                    auto url = result.url ();
                    player_manager->download (fname, url, guild_id);
                }
        }
    else
        {
            test.close ();
            event.edit_response (edit_response);
        }

    std::thread dlt (
        [comp, prepend_name, dling, fname, guild_id, from, top, arg_slip,
         edit_response] (const dpp::interaction_create_t event,
                         yt_search::YTrack result) {
            thread_manager::DoneSetter tmds;
            auto player_manager = get_player_manager_ptr ();

            if (!player_manager)
                return;

            dpp::snowflake user_id = event.command.usr.id;
            auto guild_player = player_manager->create_player (guild_id);

            if (dling)
                {
                    player_manager->wait_for_download (fname);
                    event.edit_response (edit_response);
                }

            if (from)
                guild_player->from = from;

            player::MCTrack t (result);
            t.filename = fname;
            t.user_id = user_id;
            guild_player->add_track (t, top ? true : false, guild_id, dling,
                                     arg_slip);

            if (from)
                command::play::decide_play (from, guild_id, false);
            else if (get_debug_state ())
                fprintf (stderr, "[modal_p] No client to "
                                 "decide play\n");
        },
        event, result);

    thread_manager::dispatch (dlt);
}

void
_handle_form_modal_p (const dpp::form_submit_t &event)
{
    if (!event.components.size ())
        {
            fprintf (stderr, "[events::_handle_form_modal_p WARN] Form "
                             "`modal_p` doesn't contain "
                             "any components row\n");

            return;
        }

    auto comp = event.components.at (0).components.at (0);
    bool second_input = event.components.size () > 1;
    dpp::component comp_2;

    if (second_input)
        comp_2 = event.components.at (1).components.at (0);

    if (comp.custom_id.find ("que_s_track") != std::string::npos)
        {
            _handle_modal_p_que_s_track (event, comp, comp_2, second_input);
        }
}

void
_handle_page_queue_j (const dpp::form_submit_t &event,
                      const dpp::component &comp)
{
    if (!std::holds_alternative<std::string> (comp.value))
        return;

    std::string q = std::get<std::string> (comp.value);
    if (q.empty ())
        return;

    paginate::update_page (event.command.msg.id, q, event);
}

void
_handle_form_page_queue (const dpp::form_submit_t &event)
{
    if (!event.components.size ())
        {
            fprintf (stderr, "[events::_handle_form_page_queue WARN] Form "
                             "`page_queue` doesn't contain "
                             "any components row\n");

            return;
        }

    auto comp = event.components.at (0).components.at (0);

    if (comp.custom_id == "j")
        {
            _handle_page_queue_j (event, comp);
        }
}

void
on_form_submit (dpp::cluster *client)
{
    client->on_form_submit ([] (const dpp::form_submit_t &event) {
        if (get_debug_state ())
            std::cerr << "[FORM] " << event.custom_id << ' '
                      << event.command.message_id << "\n";

        if (event.custom_id == "modal_p")
            {
                _handle_form_modal_p (event);
            }
        else if (event.custom_id == "page_queue")
            {
                _handle_form_page_queue (event);
            }
    });
}
} // musicat::events
