#include "musicat/cmds/download.h"
#include "musicat/cmds.h"
#include "musicat/cmds/play.h"
#include "musicat/mctrack.h"
#include <sys/stat.h>

namespace musicat::command::download
{
// ============ PRIVATE ============
bool
fileHasError (const dpp::interaction_create_t &event,
              const std::string &fullpath)
{
    struct stat filestat;

    if (stat (fullpath.c_str (), &filestat) != 0)
        {
            fprintf (stderr,
                     "[download::slash_run ERROR] Read failure to "
                     "upload: '%s'\n",
                     fullpath.c_str ());

            event.edit_response ("`[ERROR]` Read failure");

            return true;
        }

    if (!(filestat.st_mode & S_IFREG))
        {
            fprintf (stderr,
                     "[download::slash_run ERROR] Invalid file "
                     "type to upload: '%d'\n",
                     filestat.st_mode);

            event.edit_response ("`[FATAL]` Invalid file type");

            return true;
        }

    if (filestat.st_size > (8 * 1000000))
        {
            event.edit_response ("`[ERROR]` File size is larger than 8MB");
            return true;
        }

    return false;
}

void
e_re_no_track (const dpp::interaction_create_t &event)
{
    event.reply ("Provide `track`");
}

// ============ PRIVATE END ============

namespace autocomplete
{
void
track (const dpp::autocomplete_t &event, std::string param)
{
    // simply run the autocomplete of query argument of the play command
    // it's exactly the same expected result
    play::autocomplete::query (event, param);
}
} // autocomplete

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("download", "Download [a track]", sha_id)
        .add_option (dpp::command_option (dpp::co_string, "track",
                                          "Track [to download]")
                         .set_auto_complete (true));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return;
        }

    std::string filename = "";
    get_inter_param (event, "track", &filename);
    auto guild_id = event.command.guild_id;

    // filename
    std::string fname = "";
    // path to file
    std::string fullpath = "";

    if (!filename.empty ())
        {
            auto find_result = play::find_track (
                false, filename, player_manager, guild_id, true);

            switch (find_result.second)
                {
                case -1:
                case 1:
                    event.edit_response ("Can't find anything");
                    return;
                case 0:
                    break;
                default:
                    fprintf (stderr,
                             "[download::slash_run WARN] Unhandled find_track "
                             "return status: %d\n",
                             find_result.second);
                }

            auto result = find_result.first;

            fname = play::get_filename_from_result (result);

            auto download_result
                = play::track_exist (fname, mctrack::get_url (result),
                                     player_manager, true, guild_id, true);

            bool dling = download_result.first;

            fullpath = get_music_folder_path () + fname;

            switch (download_result.second)
                {
                case 2:
                    event.edit_response ("`[ERROR]` Unable to find track");

                    return;
                case 1:
                    if (dling)
                        {
                            event.edit_response ("Can't find anything, maybe "
                                                 "play the song first?");
                            return;
                        }
                    else
                        {
                            if (fileHasError (event, fullpath))
                                return;

                            // proceed upload
                            event.edit_response (
                                std::string ("Uploading: ")
                                + mctrack::get_title (result));
                        }
                case 0:
                    break;
                default:
                    fprintf (stderr,
                             "[download::slash_run WARN] Unhandled "
                             "track_exist return "
                             "status: %d\n",
                             download_result.second);
                }
        }
    else // no track argument specified, lets download current playing song if
         // any
        {
            auto guild_player = player_manager->get_player (guild_id);
            if (!guild_player)
                return e_re_no_track (event);

            auto conn = event.from->get_voice (guild_id);

            std::lock_guard<std::mutex> lk (guild_player->t_mutex);

            if (!guild_player->queue.size ()
                || guild_player->current_track.raw.is_null () || !conn
                || !conn->voiceclient
                || !conn->voiceclient
                        ->is_playing ()) // if there's no currently playing
                                         // track
                {

                    return e_re_no_track (event);
                }

            auto &track = guild_player->current_track;
            fname = play::get_filename_from_result (track);
            fullpath = get_music_folder_path () + fname;
        }

    // by the time it got here, fname and fullpath mustn't be empty
    event.thinking ();

    dpp::message msg (event.command.channel_id, "");
    msg.add_file (fname, dpp::utility::read_file (fullpath));
    event.edit_response (msg);
}
} // musicat::command::download
