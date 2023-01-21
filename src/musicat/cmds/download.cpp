#include "musicat/cmds.h"
#include <sys/stat.h>

namespace musicat
{
namespace command
{
namespace download
{
// ============ PRIVATE ============
bool
fileHasError (const dpp::interaction_create_t &event, const std::string& fullpath)
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
track (const dpp::autocomplete_t &event, std::string param,
       player::player_manager_ptr player_manager)
{
    play::autocomplete::query(event, param, player_manager);
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
slash_run (const dpp::interaction_create_t &event, player::player_manager_ptr player_manager)
{
    std::string filename = "";
    get_inter_param (event, "track", &filename);
    auto guild_id = event.command.guild_id;

    std::string fname = "";
    std::string fullpath = "";

    /* !DONE; ==== // get current playing track instead when no track specified */

    if (filename.length ())
        {
            auto find_result = play::find_track (false, filename, player_manager,
                                           true, guild_id, true);

            switch (find_result.second)
                {
                case -1:
                case 1:
                    event.edit_response ("Can't find anything");
                    return;
                case 0:
                    break;
                default:
                    fprintf(stderr, "[download::slash_run WARN] Unhandled find_track return status: %d\n", find_result.second);
                }

            auto result = find_result.first;

            fname = play::get_filename_from_result (result);

            auto download_result = play::track_exist (fname, result.url (), player_manager,
                                                      true, guild_id, true);
            bool dling = download_result.first;

            fullpath = get_music_folder_path () + fname;
            
            switch (download_result.second)
                {
                case 1:
                    if (dling)
                        {
                            event.edit_response (
                                "Can't find anything, maybe play the song first?");
                            return;
                        }
                    else
                        {
                            if (fileHasError (event, fullpath)) return;

                            // proceed upload
                            event.edit_response (std::string ("Uploading: ")
                                                 + result.title ());
                        }
                case 0:
                    break;
                default:
                    fprintf (stderr,
                             "[download::slash_run WARN] Unhandled track_exist return "
                             "status: %d\n",
                             download_result.second);
                }
        }
    else
        {
            auto conn = event.from->get_voice (guild_id);
            auto player = player_manager->get_player (guild_id);
            if (player)
                {
                    std::lock_guard<std::mutex> lk (player->q_m);
                    if (player->queue.size ()
                        && conn && conn->voiceclient
                        && conn->voiceclient->is_playing ()) // if there's currently playing track
                        {
                            auto& track = player->queue.front ();
                            fname = play::get_filename_from_result (track);
                            fullpath = get_music_folder_path () + fname;
                        }
                    else
                        return e_re_no_track (event);
                }
            else
                return e_re_no_track (event);
        }

    // by the time it got here, fname and fullpath mustn't be empty
    event.thinking();

    // create a message
    dpp::message msg (event.command.channel_id, "");

    // attach the file to the message
    msg.add_file (fname, dpp::utility::read_file (fullpath));

    // send the message
    event.edit_response (msg);
}
} // download
} // command
} // musicat
