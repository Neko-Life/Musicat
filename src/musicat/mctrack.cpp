#include "musicat/mctrack.h"
#include "musicat/YTDLPTrack.h"
#include "musicat/player.h"

#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/child/ytdlp.h"
#include "musicat/musicat.h"
#include "musicat/util/base64.h"

namespace musicat::mctrack
{
int
get_track_flag (const nlohmann::json &data)
{
    if (!data.is_object ())
        return 0;

    auto data_end = data.end ();
    int flag = player::TRACK_MC;

    if (is_short (data))
        flag |= player::TRACK_SHORT;

    if (data.find ("ie_key") != data_end)
        flag |= player::TRACK_YTDLP_SEARCH;

    if (data.find ("extractor_key") != data_end)
        flag |= player::TRACK_YTDLP_DETAILED;

    // !TODO: add non-youtube track flag, streaming (youtube n non-yt) and
    // generic download

    return flag;
}

int
get_track_flag (const player::MCTrack &track)
{
    const nlohmann::json &data = track.raw;

    return get_track_flag (data);
}

int
get_track_flag (const yt_search::audio_info_t &info)
{
    const nlohmann::json &data = info.raw;

    return get_track_flag (data);
}

bool
is_YTDLPTrack (const player::MCTrack &track)
{
    int flag = get_track_flag (track);

    return is_YTDLPTrack (flag);
}

bool
is_YTDLPTrack (int track_flag)
{
    return track_flag & player::TRACK_YTDLP_SEARCH
           || track_flag & player::TRACK_YTDLP_DETAILED;
}

bool
is_YTDLPTrack (const yt_search::audio_info_t &info)
{
    int flag = get_track_flag (info);

    return is_YTDLPTrack (flag);
}

std::string
get_title (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_title (track);

    return track.title ();
}

std::string
get_title (const yt_search::YTrack &track)
{
    return track.title ();
}

uint64_t
get_duration (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_duration (track);

    if (is_YTDLPTrack (track.info))
        return YTDLPTrack::get_duration (track.info);

    return track.info.duration ();
}

std::string
get_thumbnail (player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_thumbnail (track);

    return track.bestThumbnail ().url;
}

std::string
get_description (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_description (track);

    return track.snippetText ();
}

std::string
get_url (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_url (track);

    return track.url ();
}

std::string
get_url (const yt_search::YTrack &track)
{
    return track.url ();
}

std::string
get_id (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_id (track);

    return track.id ();
}

std::string
get_id (const yt_search::YTrack &track)
{
    return track.id ();
}

std::string
get_length_str (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_length_str (track);

    return track.length ();
}

std::string
get_channel_name (const player::MCTrack &track)
{
    if (is_YTDLPTrack (track))
        return YTDLPTrack::get_channel_name (track);

    return track.channel ().name;
}

bool
is_url_shorts (const std::string_view &str)
{
    return str.find ("/shorts/") != std::string::npos;
}

bool
is_short (const nlohmann::json &data)
{
    return is_url_shorts (YTDLPTrack::get_url (data));
}

bool
is_short (const player::MCTrack &track)
{
    return is_url_shorts (mctrack::get_url (track));
}

nlohmann::json
fetch (const search_option_t &options)
{
    // id, ytdlp_util_exe, ytdlp_lib_path and ytdlp_query
    namespace cc = child::command;

    std::string q = options.query;

    if (q.empty ())
        {
            fprintf (stderr, "[mctrack::fetch ERROR] Empty query\n");
            return nullptr;
        }

    if (!options.is_url)
        q = "ytsearch" + std::to_string (options.max_entries) + ":" + q;

    std::string qid = util::base64::encode (q);

    std::string ytdlp_cmd
        = cc::command_options_keys_t.command + '='
          + cc::command_execute_commands_t.call_ytdlp + ';'
          + cc::command_options_keys_t.id + '='
          + cc::sanitize_command_value (qid) + ';'
          + cc::command_options_keys_t.ytdlp_util_exe + '='
          + cc::sanitize_command_value (get_ytdlp_util_exe ()) + ';'
          + cc::command_options_keys_t.ytdlp_lib_path + '='
          + cc::sanitize_command_value (get_ytdlp_lib_path ()) + ';'
          + cc::command_options_keys_t.ytdlp_query + '='
          + cc::sanitize_command_value (q) + ';';

    if (options.max_entries != YDLP_DEFAULT_MAX_ENTRIES)
        {
            ytdlp_cmd += cc::command_options_keys_t.ytdlp_max_entries + '='
                         + std::to_string (options.max_entries) + ';';
        }

    const std::string exit_cmd
        = cc::command_options_keys_t.id + '=' + qid + ';'
          + cc::command_options_keys_t.command + '='
          + cc::command_execute_commands_t.shutdown + ';';

    cc::send_command (ytdlp_cmd);

    int status = cc::wait_slave_ready (qid, 10);

    if (status == child::worker::ready_status_t.ERR_SLAVE_EXIST)
        {
            // status won't be 0 if this block is executed
            const std::string force_exit_cmd
                = exit_cmd + cc::command_options_keys_t.force + "=1;";

            cc::send_command (force_exit_cmd);
        }

    if (status != 0)
        {
            fprintf (
                stderr,
                "[mctrack::fetch ERROR] Fetch query already in progress\n");
            return nullptr;
        }

    std::string out_fifo_path = child::ytdlp::get_ytdout_fifo_path (qid);

    int out_fifo = open (out_fifo_path.c_str (), O_RDONLY);

    if (out_fifo < 0)
        {
            fprintf (stderr,
                     "[mctrack::fetch ERROR] Failed to open outfifo\n");
            return nullptr;
        }

    char cmd_buf[CMD_BUFSIZE + 1];
    ssize_t sizread = read (out_fifo, cmd_buf, CMD_BUFSIZE);
    cmd_buf[sizread] = '\0';

    if (get_debug_state ())
        fprintf (stderr, "[mctrack::fetch] Received result: qid(%s)\n'%s'\n",
                 qid.c_str (), cmd_buf);

    cc::command_options_t opt = cc::create_command_options ();
    cc::parse_command_to_options (cmd_buf, opt);

    // fprintf (stderr, "id: %s\n", opt.id.c_str ());
    // fprintf (stderr, "command: %s\n", opt.command.c_str ());
    // fprintf (stderr, "file_path: %s\n", opt.file_path.c_str ());

    cc::send_command (exit_cmd);

    if (opt.file_path.empty ())
        {
            fprintf (stderr, "[mctrack::fetch ERROR] No result file\n");
            return nullptr;
        }

    std::ifstream scs (opt.file_path);
    if (!scs.is_open ())
        {
            fprintf (stderr,
                     "[mctrack::fetch ERROR] Unable to open result file\n");
            return nullptr;
        }

    nlohmann::json json_res;

    scs >> json_res;
    scs.close ();

    return json_res;
}

} // musicat::mctrack
