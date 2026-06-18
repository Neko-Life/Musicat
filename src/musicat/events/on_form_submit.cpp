#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/pagination.h"
#include "musicat/player.h"
#include "musicat/storage.h"
#include "musicat/thread_manager.h"
#include "musicat/util_response.h"
#include <cstdio>
#include <exception>

namespace musicat::events
{

// unused, small test to use the default download thread for now
void
_run_download_thread (const bool dling, const std::string &fname, const dpp::snowflake &guild_id, const bool top, const int64_t &arg_slip,
                      const std::string &edit_response, const dpp::form_submit_t &event, const player::MCTrack &result)
{
    try
        {
            auto *player_manager = get_player_manager_ptr ();

            if (!player_manager)
                return;

            dpp::snowflake user_id = event.command.usr.id;
            auto guild_player = player_manager->create_player (guild_id);
            auto *from = event.from ();
            guild_player->set_shard (from);

            if (dling)
                {
                    // waits for a while here ...
                    player_manager->wait_for_download (fname);
                    event.edit_response (edit_response);
                }

            player::MCTrack t (result);
            t.filename = fname;
            t.user_id = user_id;

            guild_player->add_track (t, top, guild_id, dling, arg_slip);

            from = event.from ();
            if (from)
                player::decide_play (from, guild_id, false);
            else if (get_debug_state ())
                fprintf (stderr, "[modal_p] No client to decide play\n");
        }
    catch (const std::exception &e)
        {
            fprintf (stderr, "[events::_handle_modal_p_que_s_track ::dlt ERROR] %s\n", e.what ());
        }
}

dpp::task<void>
_handle_modal_p_que_s_track (const dpp::form_submit_t &event, const dpp::component &comp, dpp::component &comp_2, bool &second_input)
{
    try
        {
            if (!std::holds_alternative<std::string> (comp.value))
                co_return;

            int64_t pos = 0;
            {
                std::string q = std::get<std::string> (comp.value);
                if (q.empty ())
                    co_return;

                sscanf (q.c_str (), "%ld", &pos);
                if (pos < 1)
                    co_return;
            }

            bool top = comp.custom_id.find ("top") != std::string::npos;

            int64_t arg_slip = 0;
            if (second_input)
                {
                    if (!std::holds_alternative<std::string> (comp_2.value))
                        co_return;

                    std::string q = std::get<std::string> (comp_2.value);
                    if (q.empty ())
                        co_return;

                    sscanf (q.c_str (), "%ld", &arg_slip);

                    if (arg_slip < 1)
                        co_return;

                    if (arg_slip == 1)
                        top = true;
                }

            auto storage = storage::get (event.command.message_id);
            if (!storage.has_value ())
                {
                    dpp::message m ("It seems like this result is outdated, try make a new search");

                    m.flags |= dpp::m_ephemeral;
                    event.reply (m);
                    co_return;
                }

            auto tracks = std::any_cast<std::vector<player::MCTrack> > (storage);
            if (tracks.size () < (size_t)pos)
                co_return;

            auto result = tracks.at (pos - 1);

            auto guild_id = event.command.guild_id;

            const std::string prepend_name = util::response::str_mention_user (event.command.usr.id);

            std::string edit_response = prepend_name + util::response::reply_added_track (mctrack::get_title (result), top ? 1 : arg_slip);

            dpp::async thinking = event.co_thinking ();

            const std::string fname = player::get_filename_from_result (result);

            bool dling = false;

            std::ifstream test (get_music_folder_path () + fname, std::ios_base::in | std::ios_base::binary);

            if (!test.is_open ())
                {
                    dling = true;
                    co_await thinking;
                    event.edit_response (prepend_name + util::response::reply_downloading_track (mctrack::get_title (result)));

                    auto player_manager = get_player_manager_ptr ();

                    if (player_manager
                        && player_manager->waiting_file_download.find (fname) == player_manager->waiting_file_download.end ())
                        {
                            auto url = mctrack::get_url (result);
                            player_manager->download (fname, url, guild_id);
                        }
                }
            else
                {
                    test.close ();
                    co_await thinking;
                    event.edit_response (edit_response);
                }

            std::thread dlt (
                [dling, fname, guild_id, top, arg_slip, edit_response, event, result] ()
                    {
                        thread_manager::DoneSetter tmds;
                        player::run_download_thread (event.from ()->shard_id, get_sha_id (), dling, fname, top, true, guild_id, false,
                                                     arg_slip, event, result, edit_response);
                    });

            thread_manager::dispatch (dlt);
        }
    catch (const std::exception &e)
        {
            fprintf (stderr, "[events::_handle_modal_p_que_s_track ERROR] %s\n", e.what ());
        }
}

void
print_components(const std::vector<dpp::component> &comps) {
    fprintf(stderr, "\nvvvvv comps size: %ld vvvvv\n", comps.size());

    int idx = 0;
    for (const auto &c : comps) {
        fprintf(stderr, "===== index %d =====\n", idx++);

        fprintf(stderr, "type: %d\n", c.type);
        fprintf(stderr, "label: %s\n", c.label.c_str());
        fprintf(stderr, "custom_id: %s\n", c.custom_id.c_str());
        fprintf(stderr, "placeholder: %s\n", c.placeholder.c_str());
        fprintf(stderr, "content: %s\n", c.content.c_str());
        fprintf(stderr, "description: %s\n", c.description.c_str());

        fprintf(stderr, "===== end index %d =====\n", idx++);
    }

    fprintf(stderr, "^^^^^ end print comps ^^^^^\n");
}

dpp::task<void>
_handle_form_modal_p (const dpp::form_submit_t &event)
{
    if (!event.components.size ())
        {
            fprintf (stderr, "[events::_handle_form_modal_p WARN] Form `modal_p` doesn't contain any components row\n");

            co_return;
        }

    const bool debug = get_debug_state();
    if (debug) print_components(event.components);

    auto comp = event.components.at (0);
    bool second_input = event.components.size () > 1;
    dpp::component comp_2;

    if (second_input)
        comp_2 = event.components.at (1);

    if (comp.custom_id.find ("que_s_track") != std::string::npos)
        {
            co_await _handle_modal_p_que_s_track (event, comp, comp_2, second_input);
        }
}

void
_handle_page_queue_j (const dpp::form_submit_t &event, const dpp::component &comp)
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
            fprintf (stderr, "[events::_handle_form_page_queue WARN] Form `page_queue` doesn't contain any components row\n");

            return;
        }

    auto comp = event.components.at (0);

    if (comp.custom_id == "j")
        {
            _handle_page_queue_j (event, comp);
        }
}

void
_handle_form_loop_mode (const dpp::form_submit_t &event)
{
    if (!event.components.size ())
        {
            fprintf (stderr, "[events::_handle_form_loop_mode WARN] Form `loop_mode` doesn't contain any components row\n");

            return;
        }

    /*command::loop::handle_button_modal_dialog (event);*/
}

void
on_form_submit (dpp::cluster *client)
{
    client->on_form_submit (
        [] (const dpp::form_submit_t &event) -> dpp::task<void>
            {
                if (get_debug_state ())
                    std::cerr << "[FORM] " << event.custom_id << ' ' << event.command.message_id << "\n";

                if (event.custom_id == "modal_p")
                    {
                        co_await _handle_form_modal_p (event);
                    }
                else if (event.custom_id == "page_queue")
                    {
                        _handle_form_page_queue (event);
                    }
                else if (event.custom_id == "loop_mode")
                    {
                        _handle_form_loop_mode (event);
                    }
            });
}
} // musicat::events
