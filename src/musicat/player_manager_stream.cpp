#include "musicat/audio_processing.h"
#include "musicat/child.h"
#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/config.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include <memory>
#include <string>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef MUSICAT_USE_PCM

#define STREAM_BUFSIZ STREAM_BUFSIZ_PCM
#define CHUNK_READ CHUNK_READ_PCM
#define DRAIN_CHUNK DRAIN_CHUNK_PCM

#else

#include <oggz/oggz.h>

#define STREAM_BUFSIZ STREAM_BUFSIZ_OPUS
#define CHUNK_READ CHUNK_READ_OPUS
#define DRAIN_CHUNK DRAIN_CHUNK_OPUS

#endif

// correct frame size with timescale for dpp
#define STREAM_BUFSIZ_PCM dpp::send_audio_raw_max_length
inline constexpr long CHUNK_READ_PCM = BUFSIZ * 2;
inline constexpr long DRAIN_CHUNK_PCM = BUFSIZ / 2;

inline constexpr long STREAM_BUFSIZ_OPUS = BUFSIZ / 8;
inline constexpr long CHUNK_READ_OPUS = BUFSIZ / 2;
inline constexpr long DRAIN_CHUNK_OPUS = BUFSIZ / 4;

namespace musicat::player
{
namespace cc = child::command;

struct mc_oggz_user_data
{
    dpp::discord_voice_client *voice_client;
    MCTrack &track;
    bool &debug;
};

#if !defined(MUSICAT_USE_PCM)
// !TODO: delete this
struct run_stream_loop_states_t
{
    dpp::discord_voice_client *&v;
    player::MCTrack &track;
    dpp::snowflake &server_id;
    void /*OGGZ*/ *&track_og;
    bool &running_state;
    bool &is_stopping;
    bool &debug;
};
#endif

static effect_states_list_t effect_states_list = {};
std::mutex effect_states_list_m; // EXTERN_VARIABLE

class EffectStatesListing
{
    const dpp::snowflake guild_id;
    bool already_exist;

  public:
    EffectStatesListing (
        const dpp::snowflake &guild_id,
        handle_effect_chain_change_states_t *effect_states_ptr)
        : guild_id (guild_id), already_exist (false)
    {
        std::lock_guard lk (effect_states_list_m);

        auto i = effect_states_list.begin ();
        while (i != effect_states_list.end ())
            {
                if ((*i)->guild_player->guild_id != guild_id)
                    {
                        i++;
                        continue;
                    }

                already_exist = true;
                break;
            }

        if (!already_exist)
            {
                effect_states_list.push_back (effect_states_ptr);
            }

        else
            std::cerr << "[musicat::player::EffectStatesListing ERROR] "
                         "Effect States already exist: "
                      << guild_id << '\n';
    }

    ~EffectStatesListing ()
    {
        if (already_exist)
            return;

        std::lock_guard lk (effect_states_list_m);

        auto i = effect_states_list.begin ();
        while (i != effect_states_list.end ())
            {
                if ((*i)->guild_player->guild_id != guild_id)
                    {
                        i++;
                        continue;
                    }

                i = effect_states_list.erase (i);
            }
    }
};

std::string
get_ffmpeg_vibrato_args (bool has_f, bool has_d,
                         std::shared_ptr<Player> &guild_player)
{
    std::string v_args;

    if (has_f)
        {
            v_args += "f=" + std::to_string (guild_player->vibrato_f);
        }

    if (has_d)
        {
            if (has_f)
                v_args += ':';

            int64_t nd = guild_player->vibrato_d;
            v_args += "d=" + std::to_string (nd > 0 ? (float)nd / 100 : nd);
        }

    return v_args;
}

std::string
get_ffmpeg_tremolo_args (bool has_f, bool has_d,
                         std::shared_ptr<Player> &guild_player)
{
    std::string v_args;

    if (has_f)
        {
            v_args += "f=" + std::to_string (guild_player->tremolo_f);
        }

    if (has_d)
        {
            if (has_f)
                v_args += ':';

            int64_t nd = guild_player->tremolo_d;
            v_args += "d=" + std::to_string (nd > 0 ? (float)nd / 100 : nd);
        }

    return v_args;
}

std::string
get_ffmpeg_pitch_args (int pitch)
{
    if (pitch == 0)
        return "";

    constexpr int64_t samp_per_percent = 24000 / 100;
    constexpr double tempo_per_percent = 0.5 / 100;

    int64_t sample = 48000 + (pitch * (-samp_per_percent));
    double tempo = 1.0 + ((double)pitch * (-tempo_per_percent));

    /*
        100=24000,0.5=-24000,-0.5=48000+(100*(-(24000/100))),1.0+(100*(-(0.5/100)))
        0=48000,1.0
        -100=72000,1.5=+24000,+0.5=48000+(-100*(-(24000/100))),1.0+(-100*(-(0.5/100)))
        -200=96000,2.0=+48000,+1.0=48000+(-200*(-(24000/100))),1.0+(-200*(-(0.5/100)))
        -300=120000,2.5=+72000,+1.5
        -400=144000,3.0=+96000,+2.0
    */

    return "aresample=" + std::to_string (sample)
           + ",atempo=" + std::to_string (tempo);
}

handle_effect_chain_change_states_t *
get_effect_states (const dpp::snowflake &guild_id)
{
    auto i = effect_states_list.begin ();
    while (i != effect_states_list.end ())
        {
            if ((*i)->guild_player->guild_id != guild_id)
                {
                    i++;
                    continue;
                }

            return *i;
        }

    return nullptr;
}

effect_states_list_t *
get_effect_states_list ()
{
    return &effect_states_list;
}

void
handle_effect_chain_change (handle_effect_chain_change_states_t &states)
{
    bool track_seek_queried = !states.track.seek_to.empty ();

    if (track_seek_queried)
        {
            std::string cmd = cc::command_options_keys_t.command + '='
                              + cc::command_options_keys_t.seek + ';'
                              + cc::command_options_keys_t.seek + '='
                              + states.track.seek_to + ';';

            cc::write_command (cmd, states.command_fd, "Manager::stream");

            // clear voice_client audio buffer
            states.vc->stop_audio ();

            states.track.seek_to = "";

            // drain current buffer while waiting for notification

            struct pollfd datapfds[1];
            datapfds[0].events = POLLIN;
            datapfds[0].fd = states.read_fd;

            // struct pollfd notifpfds[1];
            // notifpfds[0].events = POLLIN;
            // notifpfds[0].fd = states.notification_fd;

            // notification buffer
            // char nbuf[CMD_BUFSIZE + 1];
            // drain buffer
            uint8_t buffer[4];
            short revents = 0;

            while (true)
                {
                    bool has_data
                        = (poll (datapfds, 1, 100) > 0)
                          && ((revents = datapfds[0].revents) & POLLIN);

                    if (revents & POLLHUP || revents & POLLERR
                        || revents & POLLNVAL)
                        {
                            std::cerr
                                << "[Manager::stream ERROR] POLL SEEK: gid("
                                << states.vc->server_id << ") cid("
                                << states.vc->channel_id << ")\n";

                            break;
                        }

                    if (has_data)
                        {
                            // read 4 by 4 as stereo pcm frame size says so
                            ssize_t rsiz = read (states.read_fd, buffer, 4);

                            // this is the notification message to stop reading
                            // i write whatever i want!
                            if (rsiz == 4 && buffer[0] == 'B'
                                && buffer[1] == 'O' && buffer[2] == 'O'
                                && buffer[3] == 'B')
                                break;
                        }

                    // bool has_notification = (poll (notifpfds, 1, 0) > 0)
                    //                         && (notifpfds[0].revents &
                    //                         POLLIN);
                    // if (has_notification)
                    //     {
                    //         size_t nread_size = read
                    //         (states.notification_fd,
                    //                                   nbuf, CMD_BUFSIZE);

                    //         if (nread_size > 0)
                    //             // just ignore whatever message it has
                    //             // one job, simply break when notified
                    //             break;
                    //     }
                }
        }

    bool volume_queried = states.guild_player->set_volume != -1;

    if (volume_queried)
        {
            std::string cmd
                = cc::command_options_keys_t.command + '='
                  + cc::command_options_keys_t.volume + ';'
                  + cc::command_options_keys_t.volume + '='
                  + std::to_string (states.guild_player->set_volume) + ';';

            cc::write_command (cmd, states.command_fd, "Manager::stream");

            states.guild_player->volume = states.guild_player->set_volume;
            states.guild_player->set_volume = -1;
        }

    bool should_write_helper_chain_cmd = false;
    std::string helper_chain_cmd = cc::command_options_keys_t.command + '='
                                   + cc::command_options_keys_t.helper_chain
                                   + ';';

    bool tempo_queried = states.guild_player->set_tempo;
    if (tempo_queried)
        {
            std::string new_fx
                = states.guild_player->tempo == 1.0
                      ? ""
                      : "atempo="
                            + std::to_string (states.guild_player->tempo);

            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value (new_fx) + ';';

            helper_chain_cmd += cmd;

            states.guild_player->set_tempo = false;
            should_write_helper_chain_cmd = true;
        }
    else if (states.guild_player->tempo != 1.0)
        {
            std::string cmd
                = cc::command_options_keys_t.helper_chain + '='
                  + cc::sanitize_command_value (
                      "atempo=" + std::to_string (states.guild_player->tempo))
                  + ';';

            helper_chain_cmd += cmd;
        }

    bool pitch_queried = states.guild_player->set_pitch;
    if (pitch_queried)
        {
            std::string new_fx
                = get_ffmpeg_pitch_args (states.guild_player->pitch);

            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value (new_fx) + ';';

            helper_chain_cmd += cmd;

            states.guild_player->set_pitch = false;
            should_write_helper_chain_cmd = true;
        }
    else if (states.guild_player->pitch != 0)
        {
            std::string cmd
                = cc::command_options_keys_t.helper_chain + '='
                  + cc::sanitize_command_value (
                      get_ffmpeg_pitch_args (states.guild_player->pitch))
                  + ';';

            helper_chain_cmd += cmd;
        }

    bool equalizer_queried = !states.guild_player->set_equalizer.empty ();

    if (equalizer_queried)
        {
            std::string new_equalizer
                = states.guild_player->set_equalizer == "0"
                      ? ""
                      : states.guild_player->set_equalizer;

            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value (new_equalizer)
                              + ';';

            helper_chain_cmd += cmd;

            states.guild_player->equalizer = new_equalizer;
            states.guild_player->set_equalizer = "";
            should_write_helper_chain_cmd = true;
        }
    else if (!states.guild_player->equalizer.empty ())
        {
            std::string cmd
                = cc::command_options_keys_t.helper_chain + '='
                  + cc::sanitize_command_value (states.guild_player->equalizer)
                  + ';';

            helper_chain_cmd += cmd;
        }

    bool resample_queried = states.guild_player->set_sampling_rate;

    if (resample_queried)
        {
            std::string new_resample
                = states.guild_player->sampling_rate == -1
                      ? ""
                      : "aresample="
                            + std::to_string (
                                states.guild_player->sampling_rate);

            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value (new_resample)
                              + ';';

            helper_chain_cmd += cmd;

            states.guild_player->set_sampling_rate = false;
            should_write_helper_chain_cmd = true;
        }
    else if (states.guild_player->sampling_rate != -1)
        {
            std::string cmd
                = cc::command_options_keys_t.helper_chain + '='
                  + cc::sanitize_command_value (
                      "aresample="
                      + std::to_string (states.guild_player->sampling_rate))
                  + ';';

            helper_chain_cmd += cmd;
        }

    bool vibrato_queried = states.guild_player->set_vibrato;
    bool has_vibrato_f, has_vibrato_d;

    has_vibrato_f = states.guild_player->vibrato_f != -1;
    has_vibrato_d = states.guild_player->vibrato_d != -1;

    if (vibrato_queried)
        {

            std::string new_vibrato
                = (!has_vibrato_f && !has_vibrato_d)
                      ? ""
                      : "vibrato="
                            + get_ffmpeg_vibrato_args (has_vibrato_f,
                                                       has_vibrato_d,
                                                       states.guild_player);

            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value (new_vibrato) + ';';

            helper_chain_cmd += cmd;

            states.guild_player->set_vibrato = false;
            should_write_helper_chain_cmd = true;
        }
    else if (has_vibrato_f || has_vibrato_d)
        {
            std::string v_args = get_ffmpeg_vibrato_args (
                has_vibrato_f, has_vibrato_d, states.guild_player);

            std::string cmd
                = cc::command_options_keys_t.helper_chain + '='
                  + cc::sanitize_command_value ("vibrato=" + v_args) + ';';

            helper_chain_cmd += cmd;
        }

    bool tremolo_queried = states.guild_player->set_tremolo;
    bool has_tremolo_f, has_tremolo_d;

    has_tremolo_f = states.guild_player->tremolo_f != -1;
    has_tremolo_d = states.guild_player->tremolo_d != -1;

    if (tremolo_queried)
        {

            std::string new_tremolo
                = (!has_tremolo_f && !has_tremolo_d)
                      ? ""
                      : "tremolo="
                            + get_ffmpeg_tremolo_args (has_tremolo_f,
                                                       has_tremolo_d,
                                                       states.guild_player);

            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value (new_tremolo) + ';';

            helper_chain_cmd += cmd;

            states.guild_player->set_tremolo = false;
            should_write_helper_chain_cmd = true;
        }
    else if (has_tremolo_f || has_tremolo_d)
        {
            std::string v_args = get_ffmpeg_tremolo_args (
                has_tremolo_f, has_tremolo_d, states.guild_player);

            std::string cmd
                = cc::command_options_keys_t.helper_chain + '='
                  + cc::sanitize_command_value ("tremolo=" + v_args) + ';';

            helper_chain_cmd += cmd;
        }

    bool earwax_queried
        = states.guild_player->set_earwax != states.guild_player->earwax;

    if (earwax_queried)
        {
            std::string new_fx
                = states.guild_player->set_earwax == false ? "" : "earwax";

            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value (new_fx) + ';';

            helper_chain_cmd += cmd;

            states.guild_player->earwax = states.guild_player->set_earwax;

            should_write_helper_chain_cmd = true;
        }
    else if (states.guild_player->earwax)
        {
            std::string cmd = cc::command_options_keys_t.helper_chain + '='
                              + cc::sanitize_command_value ("earwax") + ';';

            helper_chain_cmd += cmd;
        }

    if (!should_write_helper_chain_cmd)
        return;

    cc::write_command (helper_chain_cmd, states.command_fd, "Manager::stream");
}

#if !defined(MUSICAT_USE_PCM)
void
run_stream_loop (Manager *manager, run_stream_loop_states_t &states,
                 handle_effect_chain_change_states_t &effect_states)
{
    float dpp_audio_buffer_length_second = get_stream_buffer_size ();

    while ((states.running_state = get_running_state ()) && states.v
           && !states.v->terminating)
        {
            if ((states.is_stopping
                 = manager->is_stream_stopping (states.server_id)))
                break;

            states.debug = get_debug_state ();

            handle_effect_chain_change (effect_states);

            const long read_bytes = oggz_read (states.track_og, CHUNK_READ);

            states.track.current_byte += read_bytes;

            if (states.debug)
                std::cerr << "[Manager::stream] "
                             "[guild_id] [size] "
                             "[chunk] [read_bytes]: "
                          << states.server_id << ' ' << states.track.filesize
                          << ' ' << read_bytes << '\n';

            while ((states.running_state = get_running_state ()) && states.v
                   && !states.v->terminating
                   && states.v->get_secs_remaining ()
                          > dpp_audio_buffer_length_second)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (
                        SLEEP_ON_BUFFER_THRESHOLD_MS));
                }

            // eof
            if (!read_bytes)
                break;
        }
}

#endif

constexpr const char *msprrfmt
    = "[Manager::stream ERROR] Processor not ready or exited: %s\n";

void
Manager::stream (dpp::discord_voice_client *v, player::MCTrack &track)
{
    const std::string &fname = track.filename;

    dpp::snowflake server_id = 0;
    std::chrono::high_resolution_clock::time_point start_time;

    const std::string music_folder_path = get_music_folder_path ();
    const std::string file_path = music_folder_path + fname;

    if (v && !v->terminating && v->is_ready ())
        {
            bool debug = get_debug_state ();

            server_id = v->server_id;
            auto guild_player
                = server_id ? this->get_player (server_id) : nullptr;

            if (!server_id || !guild_player)
                throw 2;

            guild_player->tried_continuing = false;

            FILE *ofile = fopen (file_path.c_str (), "r");

            if (!ofile)
                {
                    std::filesystem::create_directory (music_folder_path);
                    throw 2;
                }

            struct stat ofile_stat;
            if (fstat (fileno (ofile), &ofile_stat) != 0)
                {
                    fclose (ofile);
                    ofile = NULL;
                    throw 2;
                }

            fclose (ofile);
            ofile = NULL;

            track.filesize = ofile_stat.st_size;

            std::string server_id_str = std::to_string (server_id);
            std::string slave_id = "processor-" + server_id_str;

            std::string cmd
                = cc::command_options_keys_t.id + '=' + slave_id + ';'
                  + cc::command_options_keys_t.guild_id + '=' + server_id_str

                  + ';' + cc::command_options_keys_t.command + '='
                  + cc::command_execute_commands_t.create_audio_processor
                  + ';';

            if (debug)
                {
                    cmd += cc::command_options_keys_t.debug + "=1;";
                }

            cmd += cc::command_options_keys_t.file_path + '='
                   + cc::sanitize_command_value (file_path) + ';'

                   + cc::command_options_keys_t.volume + '='
                   + std::to_string (guild_player->volume) + ';';

            if (guild_player->tempo != 1.0)
                cmd += cc::command_options_keys_t.helper_chain + '='
                       + cc::sanitize_command_value (
                           "atempo=" + std::to_string (guild_player->tempo))
                       + ';';

            if (guild_player->pitch != 0)
                cmd += cc::command_options_keys_t.helper_chain + '='
                       + cc::sanitize_command_value (
                           get_ffmpeg_pitch_args (guild_player->pitch))
                       + ';';

            if (!guild_player->equalizer.empty ())
                cmd += cc::command_options_keys_t.helper_chain + '='
                       + cc::sanitize_command_value (guild_player->equalizer)
                       + ';';

            if (guild_player->sampling_rate != -1)
                cmd += cc::command_options_keys_t.helper_chain + '='
                       + cc::sanitize_command_value (
                           "aresample="
                           + std::to_string (guild_player->sampling_rate))
                       + ';';

            bool has_vibrato_f = guild_player->vibrato_f != -1,
                 has_vibrato_d = guild_player->vibrato_d != -1;

            if (has_vibrato_f || has_vibrato_d)
                {
                    std::string v_args = get_ffmpeg_vibrato_args (
                        has_vibrato_f, has_vibrato_d, guild_player);

                    cmd += cc::command_options_keys_t.helper_chain + '='
                           + cc::sanitize_command_value ("vibrato=" + v_args)
                           + ';';
                }

            bool has_tremolo_f = guild_player->tremolo_f != -1,
                 has_tremolo_d = guild_player->tremolo_d != -1;

            if (has_tremolo_f || has_tremolo_d)
                {
                    std::string v_args = get_ffmpeg_tremolo_args (
                        has_tremolo_f, has_tremolo_d, guild_player);

                    cmd += cc::command_options_keys_t.helper_chain + '='
                           + cc::sanitize_command_value ("tremolo=" + v_args)
                           + ';';
                }

            if (guild_player->earwax)
                cmd += cc::command_options_keys_t.helper_chain + '='
                       + cc::sanitize_command_value ("earwax") + ';';

            // !TODO: convert current byte to timestamp string
            // + cc::command_options_keys_t.seek + '=' +
            // track.current_byte
            // + ';';

            const std::string exit_cmd
                = cc::command_options_keys_t.id + '=' + slave_id + ';'
                  + cc::command_options_keys_t.command + '='
                  + cc::command_execute_commands_t.shutdown + ';';

            cc::send_command (cmd);

            int status = cc::wait_slave_ready (slave_id, 10);

            if (status == child::worker::ready_status_t.ERR_SLAVE_EXIST)
                {
                    // status won't be 0 if this block is executed
                    const std::string force_exit_cmd
                        = exit_cmd + cc::command_options_keys_t.force + "=1;";

                    cc::send_command (force_exit_cmd);
                }

            if (status != 0)
                {
                    throw 2;
                }

            const std::string fifo_stream_path
                = audio_processing::get_audio_stream_fifo_path (slave_id);

            const std::string fifo_command_path
                = audio_processing::get_audio_stream_stdin_path (slave_id);

            const std::string fifo_notify_path
                = audio_processing::get_audio_stream_stdout_path (slave_id);

            // OPEN FIFOS
            int read_fd = open (fifo_stream_path.c_str (), O_RDONLY);
            if (read_fd < 0)
                {
                    throw 2;
                }

            int command_fd = open (fifo_command_path.c_str (), O_WRONLY);
            if (command_fd < 0)
                {
                    close (read_fd);
                    throw 2;
                }

            int notification_fd = open (fifo_notify_path.c_str (), O_RDONLY);
            if (notification_fd < 0)
                {
                    close (read_fd);
                    close (command_fd);
                    throw 2;
                }

            // wait for processor notification
            char nbuf[CMD_BUFSIZE + 1];
            size_t nread_size = read (notification_fd, nbuf, CMD_BUFSIZE);

            bool processor_read_ready = false;
            if (nread_size > 0)
                {
                    nbuf[nread_size] = '\0';

                    if (std::string (nbuf) == "0")
                        {
                            processor_read_ready = true;
                        }
                }

            handle_effect_chain_change_states_t effect_states
                = { guild_player, track, command_fd,     read_fd,
                    NULL,         v,     notification_fd };

            int throw_error = 0;
            bool running_state = get_running_state (), is_stopping;

            if (!processor_read_ready)
                {
                    fprintf (stderr, msprrfmt, slave_id.c_str ());
                    close (read_fd);
                    close (command_fd);
                    close (notification_fd);
                    throw 2;
                }

            EffectStatesListing esl (server_id, &effect_states);

            float dpp_audio_buffer_length_second = get_stream_buffer_size ();

            // I LOVE C++!!!

            // track.seekable = true;

            // using raw pcm need to change ffmpeg output format to s16le!
#ifdef MUSICAT_USE_PCM
            ssize_t read_size = 0;
            ssize_t current_read = 0;
            ssize_t total_read = 0;
            uint8_t buffer[STREAM_BUFSIZ];

            while ((running_state = get_running_state ())
                   && ((current_read = read (read_fd, buffer + read_size,
                                             STREAM_BUFSIZ - read_size))
                       > 0))
                {
                    read_size += current_read;
                    total_read += current_read;

                    if ((is_stopping = is_stream_stopping (server_id)))
                        break;

                    if (read_size != STREAM_BUFSIZ)
                        continue;

                    if ((debug = get_debug_state ()))
                        fprintf (stderr, "Sending buffer: %ld %ld\n",
                                 total_read, read_size);

                    if (audio_processing::send_audio_routine (
                            v, (uint16_t *)buffer, &read_size))
                        break;

                    handle_effect_chain_change (effect_states);

                    float outbuf_duration;

                    while ((running_state = get_running_state ()) && v
                           && !v->terminating
                           && ((outbuf_duration = v->get_secs_remaining ())
                               > dpp_audio_buffer_length_second))
                        {
                            // isn't very pretty for the terminal,
                            // disable for now
                            // if ((debug =
                            // get_debug_state ()))
                            //     {
                            //         static std::chrono::time_point
                            //             start_time
                            //             = std::chrono::
                            //                 high_resolution_clock::
                            //                     now ();

                            //         auto end_time = std::chrono::
                            //             high_resolution_clock::now
                            //             ();

                            //         auto done
                            //             =
                            //             std::chrono::duration_cast<
                            //                 std::chrono::
                            //                     milliseconds> (
                            //                 end_time - start_time);

                            //         start_time = end_time;

                            //         fprintf (
                            //             stderr,
                            //             "[audio_processing::send_"
                            //             "audio_routine] "
                            //             "outbuf_duration: %f >
                            //             %f\n", outbuf_duration,
                            //             DPP_AUDIO_BUFFER_LENGTH_SECOND);

                            //         fprintf (stderr,
                            //                  "[audio_processing::send_"
                            //                  "audio_routine] Delay "
                            //                  "between send: %ld "
                            //                  "milliseconds\n",
                            //                  done.count ());
                            //     }

                            handle_effect_chain_change (effect_states);

                            std::this_thread::sleep_for (
                                std::chrono::milliseconds (
                                    SLEEP_ON_BUFFER_THRESHOLD_MS));
                        }
                }

            if ((read_size > 0) && running_state && !is_stopping)
                {
                    if (debug)
                        fprintf (stderr, "Final buffer: %ld %ld\n",
                                 (total_read += read_size), read_size);

                    audio_processing::send_audio_routine (
                        v, (uint16_t *)buffer, &read_size, true);
                }

            close (read_fd);

            // using raw pcm code ends here
#else
            // using OGGZ need to change ffmpeg output format to opus!
            ofile = fdopen (read_fd, "r");

            if (!ofile)
                {
                    close (read_fd);
                    close (command_fd);
                    close (notification_fd);
                    throw 2;
                }

            OGGZ *track_og = oggz_open_stdio (ofile, OGGZ_READ);

            if (track_og)
                {
                    mc_oggz_user_data data = { v, track, debug };

                    oggz_set_read_callback (
                        track_og, -1,
                        [] (OGGZ *oggz, oggz_packet *packet, long serialno,
                            void *user_data) {
                            mc_oggz_user_data *data
                                = (mc_oggz_user_data *)user_data;

                            // if (data->debug)
                            //     fprintf (stderr, "OGGZ Read Bytes: %ld\n ",
                            //              packet->op.bytes);

                            data->voice_client->send_audio_opus (
                                packet->op.packet, packet->op.bytes);

                            // if (!data->track.seekable && packet->op.b_o_s ==
                            // 0)
                            //     {
                            //         data->track.seekable = true;
                            //     }

                            return 0;
                        },
                        (void *)&data);

                    struct run_stream_loop_states_t states = {
                        v,           track, server_id, track_og, running_state,
                        is_stopping, debug,
                    };

                    effect_states.track_og = track_og;

                    // stream loop
                    run_stream_loop (this, states, effect_states);
                }
            else
                {
                    fprintf (stderr,
                             "[Manager::stream ERROR] Can't open file for "
                             "reading: %ld '%s'\n",
                             server_id, file_path.c_str ());
                }

            // read_fd already closed along with this
            oggz_close (track_og);
            track_og = NULL;

            // using OGGZ code ends here
#endif

            close (command_fd);
            command_fd = -1;
            close (notification_fd);
            notification_fd = -1;

            if (debug)
                std::cerr << "Exiting " << server_id << '\n';

            // commented for testing purpose
            cc::send_command (exit_cmd);

            if (!running_state || is_stopping)
                {
                    // clear voice client buffer
                    v->stop_audio ();
                }

            auto end_time = std::chrono::high_resolution_clock::now ();
            auto done = std::chrono::duration_cast<std::chrono::milliseconds> (
                end_time - start_time);

            if (debug)
                {
                    fprintf (stderr, "Done streaming for %lld milliseconds\n",
                             done.count ());
                    // fprintf (stderr, "audio_processing status: %d\n",
                    // status);
                }

            if (throw_error != 0)
                {
                    throw throw_error;
                }
        }
    else
        throw 1;
}

bool
Manager::is_stream_stopping (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (sq_m);

    if (guild_id && stop_queue[guild_id])
        return true;

    return false;
}

int
Manager::set_stream_stopping (const dpp::snowflake &guild_id)
{
    std::lock_guard<std::mutex> lk (sq_m);

    if (guild_id)
        {
            stop_queue[guild_id] = true;
            return 0;
        }

    return 1;
}

int
Manager::clear_stream_stopping (const dpp::snowflake &guild_id)
{
    if (guild_id)
        {
            std::lock_guard<std::mutex> lk (sq_m);
            stop_queue.erase (guild_id);

            return 0;
        }

    return 1;
}

void
Manager::set_processor_state (std::string &server_id_str,
                              processor_state_t state)
{
    std::lock_guard<std::mutex> lk (this->as_m);

    this->processor_states[server_id_str] = state;
}

processor_state_t
Manager::get_processor_state (std::string &server_id_str)
{
    std::lock_guard<std::mutex> lk (this->as_m);
    auto i = this->processor_states.find (server_id_str);

    if (i == this->processor_states.end ())
        {
            return PROCESSOR_NULL;
        }

    return i->second;
}

bool
Manager::is_processor_ready (std::string &server_id_str)
{
    return get_processor_state (server_id_str) & PROCESSOR_READY;
}

bool
Manager::is_processor_dead (std::string &server_id_str)
{
    return get_processor_state (server_id_str) & PROCESSOR_DEAD;
}

} // musicat::player
