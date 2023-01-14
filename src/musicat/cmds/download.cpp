#include "musicat/cmds.h"

namespace musicat
{
namespace command
{
namespace download
{
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

    if (!filename.length ())
        return event.reply ("Provide `track`");

    event.thinking();

    auto guild_id = event.command.guild_id;

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

    const std::string fname = play::get_filename_from_result (result);

    auto download_result = play::track_exist (fname, result.url (), player_manager,
                                        true, guild_id, true);
    bool dling = download_result.first;
    
    switch (download_result.second)
        {
        case 1:
            if (dling)
                {
                    event.edit_response ("Can't find anything, maybe play the song first?");
                    return;
                }
            else
                {
                    event.edit_response (std::string ("Added: ")
                                         + result.title ());
                }
        case 0:
            break;
        default:
            fprintf (stderr,
                     "[download::slash_run WARN] Unhandled track_exist return status: %d\n",
                     download_result.second);
        }

    // create a message
    dpp::message msg(event.command.channel_id, "");

    // attach the file to the message
    msg.add_file(fname, dpp::utility::read_file(get_music_folder_path() + fname));

    // send the message
    event.edit_response(msg);
}
} // download
} // command
} // musicat
