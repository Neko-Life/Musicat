#include "musicat/cmds/seek.h"
#include "musicat/cmds.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include <cstdint>

namespace musicat::command::seek
{

inline constexpr char valid_tokens[] = "0123456789:.";

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("seek", "Seek [currently playing] track", sha_id)
        .add_option (dpp::command_option (
                         dpp::co_string, "to",
                         "Timestamp [to seek to]. Format: Absolute "
                         "`<[[hour:]minute:]second[.ms]>`. Example: 4:20.69",
                         true)
                         .set_max_length (14));
}

bool
is_valid_char (char c)
{
    for (size_t i = 0;
         i < ((sizeof (valid_tokens) / sizeof (valid_tokens[0])) - 1); i++)
        {
            if (c == valid_tokens[i])
                return true;
        }

    return false;
}

struct arg_to_t
{
    int64_t hour;
    int64_t minute;
    int64_t second;
    int64_t ms;
    bool valid;
};

arg_to_t
parse_arg_to (const std::string &str)
{
    arg_to_t result = { 0, 0, 0, 0, false };

    if (str.length () < 1 || str.length () > 14)
        {
            return result;
        }

    std::string list[4] = {};

    size_t last_idx = 0;
    size_t sc_count = 0;
    size_t dot_count = 0;
    int i = 0;
    for (; i < 4; i++)
        {
            std::string temp = "";

            for (auto j = (str.begin () + last_idx); j != str.end ();
                 j++, last_idx++)
                {
                    if (!is_valid_char (*j))
                        {
                            return result;
                        }

                    if (*j == ':')
                        {
                            if (dot_count != 0)
                                {
                                    return result;
                                }

                            sc_count++;

                            last_idx++;
                            break;
                        }

                    if (*j == '.')
                        {
                            dot_count++;

                            last_idx++;
                            break;
                        }

                    temp += *j;
                }

            if (temp.empty ())
                {
                    break;
                }

            list[i] = temp;
        }

    if (i > 0 && sc_count < 3 && dot_count < 2)
        {
            int subt = -1;
            if (sc_count == 2)
                {
                    if (list[0].empty ())
                        return result;

                    result.hour = stoll (list[0], NULL, 10);
                    subt = 0;
                }

            if (sc_count > 0)
                {
                    if (subt == -1)
                        subt = 1;

                    if (list[1 - subt].empty ())
                        return result;

                    result.minute = stoll (list[1 - subt], NULL, 10);
                }

            if (subt == -1)
                subt = 2;

            if (list[2 - subt].empty ())
                return result;

            result.second = stoll (list[2 - subt], NULL, 10);

            if (dot_count == 1)
                {
                    if (list[3 - subt].empty ())
                        return result;

                    result.ms = stoll (list[3 - subt], NULL, 10);
                }

            result.valid = true;
        }

    if (get_debug_state ())
        {
            fprintf (stderr,
                     "[seek::parse_arg_to] Parsed hour minute second ms "
                     "valid: %ld %ld %ld %ld %d\n",
                     result.hour, result.minute, result.second, result.ms,
                     result.valid);
        }

    return result;
}

int
run (const dpp::snowflake &guild_id, const std::string &arg_to,
     std::string &out)
{
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        {
            return -1;
        }

    const bool debug = get_debug_state ();

    arg_to_t parsed = parse_arg_to (arg_to);

    if (!parsed.valid)
        {
            out = "Invalid format, seek format should be "
                  "`[[hour:]minute:]second[.ms]`, Timestamp exceeding "
                  "duration "
                  "implies skip.\nExamples:\n`360` "
                  "-> second\n`270.629` -> second.ms\n`3:24` -> "
                  "minute:second\n`2:15:37.899` -> hour:minute:second.ms";

            return 0;
        }

    // int64_t seek_byte = -1;

    auto player = player_manager->get_player (guild_id);

    if (!util::player_has_current_track (player))
        {
            out = "I'm not playing anything";
            return 0;
        }

    // !TODO: probably add a mutex for safety just in case?
    player::MCTrack &track = player->current_track;

    uint64_t duration = mctrack::get_duration (track);

    if (!duration || !track.filesize)
        {
            out = "I'm sorry but the current track is not seek-able. "
                  "Might be missing metadata or unsupported format";

            return 0;
        }

    constexpr uint64_t second_ms = 1000;
    constexpr uint64_t minute_ms = second_ms * 60;
    constexpr uint64_t hour_ms = 60 * minute_ms;

    const uint64_t total_ms = (parsed.hour * hour_ms)
                              + (parsed.minute * minute_ms)
                              + (parsed.second * second_ms) + parsed.ms;
    if (debug)
        fprintf (stderr, "[seek::slash_run] [total_ms] [duration]: %ld %ld\n",
                 total_ms, duration);

    // skip instead of error
    // if (total_ms > duration)
    //     {
    //         event.reply ("Can't seek, seek target exceeding track
    //         duration"); return;
    //     }

    float byte_per_ms = (float)track.filesize / (float)duration;

    track.current_byte = (int64_t)(byte_per_ms * total_ms);

    if (debug)
        {
            fprintf (stderr,
                     "[seek::slash_run] [filesize] [duration] "
                     "[byte_per_ms] [seek_byte]: "
                     "%f %f %f %ld\n",
                     (float)track.filesize, (float)duration, byte_per_ms,
                     track.current_byte);
        }

    track.seek_to = arg_to;

    out = "Seeking to " + arg_to;
    return 0;
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::string arg_to = "";
    get_inter_param (event, "to", &arg_to);

    std::string out;
    int status = run (event.command.guild_id, arg_to, out);

    if (status == 0)
        event.reply (out);
}

int
button_run (const dpp::button_click_t &event,
            int (*get_to_ms) (std::string &to_ms,
                              player::track_progress &progress))
{
    auto pm_res = command::cmd_pre_get_player_manager_ready_werr (
        event.command.guild_id);

    if (pm_res.second != 0)
        return pm_res.second;

    auto player_manager = pm_res.first;
    auto guild_player = player_manager->get_player (event.command.guild_id);
    if (!guild_player || !guild_player->processing_audio)
        {
            player_manager->update_info_embed (event.command.guild_id, false,
                                               &event);
            return -2;
        }

    player::track_progress prog
        = util::get_track_progress (guild_player->current_track);

    if (prog.status == 0)
        {
            std::string to_ms;
            int status = get_to_ms (to_ms, prog);
            if (status)
                return status;

            std::string out;
            run (event.command.guild_id, to_ms, out);
        }

    player_manager->update_info_embed (event.command.guild_id, false, &event);
    return 0;
}

inline constexpr const int64_t second30 = 30 * 1000;

int
get_to_ms_rewind (std::string &to_ms, player::track_progress &progress)
{
    const bool debug = get_debug_state ();
    if (progress.current_ms < second30)
        to_ms = "0";
    else
        {
            to_ms = format_duration ((uint64_t)progress.current_ms - second30);

            if (debug)
                {
                    fprintf (stderr,
                             "[command::seek::get_to_ms_rewind] to_ms(%s)\n",
                             to_ms.c_str ());
                }
        }

    return 0;
}

int
get_to_ms_zero_less_30_err (std::string &to_ms,
                            player::track_progress &progress)
{
    if (progress.current_ms < second30)
        return 128;

    to_ms = "0";

    return 0;
}

int
get_to_ms_forward (std::string &to_ms, player::track_progress &progress)
{
    const bool debug = get_debug_state ();

    to_ms = format_duration ((uint64_t)progress.current_ms + second30);

    if (debug)
        fprintf (stderr, "[command::seek::get_to_ms_forward] to_ms(%s)\n",
                 to_ms.c_str ());

    return 0;
}

// rewind 30 sec
void
button_run_rewind (const dpp::button_click_t &event)
{
    button_run (event, get_to_ms_rewind);
}

// seek to second 0 but returns 128 if current track progress is less than 30
// second
int
button_seek_zero (const dpp::button_click_t &event)
{
    return button_run (event, get_to_ms_zero_less_30_err);
}

// forward 30 sec
void
button_run_forward (const dpp::button_click_t &event)
{
    button_run (event, get_to_ms_forward);
}

} // musicat::command::seek
