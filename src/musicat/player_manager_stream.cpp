#include "musicat/audio_config.h"
// #include "musicat/audio_processing.h"
// #include "musicat/child.h"
// #include "musicat/child/command.h"
#include "musicat/db.h"
#include "musicat/decoder.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
// #include "musicat/server/routes/get_stream.h"
// #include "musicat/server/stream.h"
#include "musicat/server/ws/player.h"
#include "musicat/thread_manager.h"
// #include "musicat/util.h"
#include "musicat/util/fs.h"
// #include "musicat/stream_codec.h"

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
// #include <shared_mutex>
#include <string>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace musicat::player
{
// namespace cc = child::command;
// namespace cw = child::worker;

// static int
// wait_for_ready_event (const dpp::snowflake &guild_id)
// {
//     auto player_manager = get_player_manager_ptr ();
//     if (!player_manager)
//         return 1;
//
//     if (player_manager->wait_for_vc_ready (guild_id) == 0)
//         {
//             auto guild_player = player_manager->get_player (guild_id);
//
//             // this should never be happening
//             if (!guild_player)
//                 return 0;
//
//             // reset current byte
//             guild_player->reset_first_track_current_byte ();
//
//             if (auto vc = guild_player->get_voice_client (); vc != nullptr)
//                 {
//                     // check stage channel routine
//                     player_manager->prepare_play_stage_channel_routine (vc, dpp::find_guild (guild_id));
//                 }
//
//             // republish playback info as we got moved to different vc session
//             server::ws::player::publish_playback_info (guild_id);
//         }
//
//     return 0;
// }

static void
handle_effect_chain_change (handle_effect_chain_change_states_t states)
{
    const std::string new_descr = states.guild_player->get_filter_descr ();
    const bool filter_queried = new_descr != states.dec.get_filter_descr ();
    const bool seek_queried = !states.track.seek_to.empty ();
    if (filter_queried)
        {
            states.dec.set_filter_descr (new_descr);
            if (states.dec.init_filters () < 0)
                throw 2;

            database::update_guild_player_config (states.guild_player->guild_id, NULL, NULL, NULL,
                                                  states.guild_player->fx_states_to_json ());
            server::ws::player::publish_fx (states.guild_player->guild_id);
        }

    if (seek_queried)
        {
            states.dec.seek (util::get_track_progress (states.track).current_ms);
            states.track.seek_to = "";
            states.guild_player->reset_first_track_current_byte ();
        }
    return;
    // const std::string dbg_str_arg = cc::get_dbg_str_arg ();
    //
    // auto vc = states.guild_player->get_voice_client ();
    // const bool has_vc = vc != nullptr;
    //
    // bool track_seek_queried = !states.track.seek_to.empty ();
    // if (track_seek_queried)
    //     {
    //         std::string cmd = cc::command_options_keys_t.command + '=' + cc::command_options_keys_t.seek + ';'
    //                           + cc::command_options_keys_t.seek + '=' + cc::sanitize_command_value (states.track.seek_to) + ';'
    //                           + dbg_str_arg;
    //
    //         cc::write_command (cmd, states.command_fd, "Manager::stream");
    //
    //         // clear voice_client audio buffer
    //         if (has_vc)
    //             vc->stop_audio ();
    //
    //         states.track.seek_to = "";
    //
    //         // drain current buffer while waiting for notification
    //
    //         struct pollfd datapfds[1];
    //         datapfds[0].events = POLLIN;
    //         datapfds[0].fd = states.read_fd;
    //
    //         // struct pollfd notifpfds[1];
    //         // notifpfds[0].events = POLLIN;
    //         // notifpfds[0].fd = states.notification_fd;
    //
    //         // notification buffer
    //         // char nbuf[CMD_BUFSIZE + 1];
    //         // drain buffer
    //         uint8_t buffer[4];
    //         short revents = 0;
    //
    //         while (true)
    //             {
    //                 bool has_data = (poll (datapfds, 1, 100) > 0) && ((revents = datapfds[0].revents) & POLLIN);
    //
    //                 if (revents & POLLHUP || revents & POLLERR || revents & POLLNVAL)
    //                     {
    //                         std::cerr << "[Manager::stream ERROR] POLL SEEK: gid(" << (has_vc ? vc->server_id.str () : "-1") << ") cid("
    //                                   << (has_vc ? vc->channel_id.str () : "-1") << ")\n";
    //
    //                         break;
    //                     }
    //
    //                 if (has_data)
    //                     {
    //                         // read 4 by 4 as stereo pcm frame size says so
    //                         ssize_t rsiz = read (states.read_fd, buffer, 4);
    //
    //                         // this is the notification message to stop reading
    //                         // i write whatever i want!
    //                         if (rsiz == 4 && buffer[0] == 'B' && buffer[1] == 'O' && buffer[2] == 'O' && buffer[3] == 'B')
    //                             break;
    //                     }
    //
    //                 // bool has_notification = (poll (notifpfds, 1, 0) > 0)
    //                 //                         && (notifpfds[0].revents &
    //                 //                         POLLIN);
    //                 // if (has_notification)
    //                 //     {
    //                 //         size_t nread_size = read
    //                 //         (states.notification_fd,
    //                 //                                   nbuf, CMD_BUFSIZE);
    //
    //                 //         if (nread_size > 0)
    //                 //             // just ignore whatever message it has
    //                 //             // one job, simply break when notified
    //                 //             break;
    //                 //     }
    //             }
    //     }
    //
    // bool volume_queried = states.guild_player->set_volume != -1;
    // if (volume_queried)
    //     {
    //         std::string cmd = cc::command_options_keys_t.command + '=' + cc::command_options_keys_t.volume + ';'
    //                           + cc::command_options_keys_t.volume + '=' + std::to_string (states.guild_player->set_volume) + ';'
    //                           + dbg_str_arg;
    //
    //         cc::write_command (cmd, states.command_fd, "Manager::stream");
    //
    //         states.guild_player->volume = states.guild_player->set_volume;
    //         states.guild_player->set_volume = -1;
    //     }
    //
    // bool should_write_helper_chain_cmd = false;
    // std::string helper_chain_cmd = cc::command_options_keys_t.command + '=' + cc::command_options_keys_t.helper_chain + ';';
    //
    // bool tempo_queried = states.guild_player->set_tempo;
    // if (tempo_queried)
    //     {
    //         std::string new_fx = states.guild_player->tempo == 1.0 ? "" : "!atempo=" + std::to_string (states.guild_player->tempo);
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (new_fx) + ';';
    //
    //         helper_chain_cmd += cmd;
    //
    //         states.guild_player->set_tempo = false;
    //         should_write_helper_chain_cmd = true;
    //     }
    //
    // bool pitch_queried = states.guild_player->set_pitch;
    // if (pitch_queried)
    //     {
    //         std::string new_fx = get_ffmpeg_pitch_args (states.guild_player->pitch);
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (new_fx) + ';';
    //
    //         helper_chain_cmd += cmd;
    //
    //         states.guild_player->set_pitch = false;
    //         should_write_helper_chain_cmd = true;
    //     }
    //
    // bool equalizer_queried = states.guild_player->set_equalizer;
    // if (equalizer_queried)
    //     {
    //         std::string new_equalizer = states.guild_player->equalizer == "0" ? "" : "superequalizer=" + states.guild_player->equalizer;
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (new_equalizer) + ';';
    //
    //         helper_chain_cmd += cmd;
    //
    //         if (new_equalizer.empty ())
    //             states.guild_player->equalizer.clear ();
    //
    //         states.guild_player->set_equalizer = false;
    //         should_write_helper_chain_cmd = true;
    //     }
    //
    // bool resample_queried = states.guild_player->set_sampling_rate;
    // if (resample_queried)
    //     {
    //         std::string new_resample
    //             = states.guild_player->sampling_rate == -1 ? "" : "aresample=" + std::to_string (states.guild_player->sampling_rate);
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (new_resample) + ';';
    //
    //         helper_chain_cmd += cmd;
    //
    //         states.guild_player->set_sampling_rate = false;
    //         should_write_helper_chain_cmd = true;
    //     }
    //
    // bool vibrato_queried = states.guild_player->set_vibrato;
    // bool has_vibrato_f, has_vibrato_d;
    //
    // has_vibrato_f = states.guild_player->vibrato_f != -1;
    // has_vibrato_d = states.guild_player->vibrato_d != -1;
    //
    // if (vibrato_queried)
    //     {
    //
    //         std::string new_vibrato = (!has_vibrato_f && !has_vibrato_d)
    //                                       ? ""
    //                                       : "vibrato=" + get_ffmpeg_vibrato_args (has_vibrato_f, has_vibrato_d, states.guild_player);
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (new_vibrato) + ';';
    //
    //         helper_chain_cmd += cmd;
    //
    //         states.guild_player->set_vibrato = false;
    //         should_write_helper_chain_cmd = true;
    //     }
    //
    // bool tremolo_queried = states.guild_player->set_tremolo;
    // bool has_tremolo_f, has_tremolo_d;
    //
    // has_tremolo_f = states.guild_player->tremolo_f != -1;
    // has_tremolo_d = states.guild_player->tremolo_d != -1;
    //
    // if (tremolo_queried)
    //     {
    //
    //         std::string new_tremolo = (!has_tremolo_f && !has_tremolo_d)
    //                                       ? ""
    //                                       : "tremolo=" + get_ffmpeg_tremolo_args (has_tremolo_f, has_tremolo_d, states.guild_player);
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (new_tremolo) + ';';
    //
    //         helper_chain_cmd += cmd;
    //
    //         states.guild_player->set_tremolo = false;
    //         should_write_helper_chain_cmd = true;
    //     }
    //
    // bool earwax_queried = states.guild_player->set_earwax;
    //
    // if (earwax_queried)
    //     {
    //         std::string new_fx = states.guild_player->earwax == false ? "" : "earwax";
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (new_fx) + ';';
    //
    //         helper_chain_cmd += cmd;
    //
    //         states.guild_player->set_earwax = false;
    //
    //         should_write_helper_chain_cmd = true;
    //     }
    //
    // if (!should_write_helper_chain_cmd)
    //     return;
    //
    // // update fx_states in db
    // // !TODO: this probably introduces latency here, need to offload it to other thread!
    // database::update_guild_player_config (states.guild_player->guild_id, NULL, NULL, NULL, states.guild_player->fx_states_to_json ());
    //
    // // check for existed non queried and add it to cmd
    //
    // if (!tempo_queried && states.guild_player->tempo != 1.0)
    //     {
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '='
    //                           + cc::sanitize_command_value ("!atempo=" + std::to_string (states.guild_player->tempo)) + ';';
    //
    //         helper_chain_cmd += cmd;
    //     }
    //
    // if (!pitch_queried && states.guild_player->pitch != 0)
    //     {
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '='
    //                           + cc::sanitize_command_value (get_ffmpeg_pitch_args (states.guild_player->pitch)) + ';';
    //
    //         helper_chain_cmd += cmd;
    //     }
    //
    // if (!equalizer_queried && !states.guild_player->equalizer.empty ())
    //     {
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '='
    //                           + cc::sanitize_command_value ("superequalizer=" + states.guild_player->equalizer) + ';';
    //
    //         helper_chain_cmd += cmd;
    //     }
    //
    // if (!resample_queried && states.guild_player->sampling_rate != -1)
    //     {
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '='
    //                           + cc::sanitize_command_value ("aresample=" + std::to_string (states.guild_player->sampling_rate)) + ';';
    //
    //         helper_chain_cmd += cmd;
    //     }
    //
    // if (!vibrato_queried && (has_vibrato_f || has_vibrato_d))
    //     {
    //         std::string v_args = get_ffmpeg_vibrato_args (has_vibrato_f, has_vibrato_d, states.guild_player);
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value ("vibrato=" + v_args) + ';';
    //
    //         helper_chain_cmd += cmd;
    //     }
    //
    // if (!tremolo_queried && (has_tremolo_f || has_tremolo_d))
    //     {
    //         std::string v_args = get_ffmpeg_tremolo_args (has_tremolo_f, has_tremolo_d, states.guild_player);
    //
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value ("tremolo=" + v_args) + ';';
    //
    //         helper_chain_cmd += cmd;
    //     }
    //
    // if (!earwax_queried && states.guild_player->earwax)
    //     {
    //         std::string cmd = cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value ("earwax") + ';';
    //
    //         helper_chain_cmd += cmd;
    //     }
    //
    // cc::write_command (helper_chain_cmd + dbg_str_arg, states.command_fd, "Manager::stream");
    // server::ws::player::publish_fx (states.guild_player->guild_id);
}

#ifdef USING_LIBOPUSENC

// this should be called
// inside the streaming thread
int
send_audio_routine (Player *guild_player, uint8_t *send_buffer, ssize_t *send_buffer_length, bool no_wait, OggOpusEnc *opus_encoder)
{
    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;

    if (!running_state || !vclient || vclient->terminating)
        return 1;

    const bool debug = get_debug_state ();

    // calculate duration
    if ((*send_buffer_length > 0))
        {
            int64_t samp_calc = guild_player->sampling_rate == -1 ? 48000 : guild_player->sampling_rate;

            // take account earwax resampling
            if (guild_player->earwax)
                samp_calc -= 3900;

            // (buffer_size / (sampling rate * channel * (bit width(16) / bit per byte(8))) * 1 second in ms) * opus byte_per_ms
            int64_t add = (int64_t)((double)((float)((float)*send_buffer_length / (samp_calc * 2 * 2) * 1000) * opus_byte_per_ms)
                                    * guild_player->tempo);
            guild_player->current_track.current_byte += add;
        }

    try
        {
            if (!opus_encoder)
                return 2;

            if (ope_encoder_write (opus_encoder, (opus_int16 *)send_buffer, (*send_buffer_length) / 4) != OPE_OK)
                return 3;
        }
    catch (const dpp::voice_exception &e)
        {
            fprintf (stderr, "[player::send_audio_routine ERROR] %s\n", e.what ());
        }

    *send_buffer_length = 0;

    return 0;
}

#else // USING_LIBOPUSENC

// this should be called
// inside the streaming thread
int
send_audio_routine (dpp::discord_voice_client *vclient, uint16_t *send_buffer, ssize_t *send_buffer_length, bool no_wait,
                    OpusEncoder *opus_encoder)
{
    // const bool debug = get_debug_state ();
    bool running_state = get_running_state ();

    if (!running_state || !vclient || vclient->terminating)
        {
            return 1;
        }

    const bool debug = get_debug_state ();

    // calculate duration
    if ((*send_buffer_length > 0))
        {
            auto player_manager = get_player_manager_ptr ();
            auto guild_player = player_manager ? player_manager->get_player (vclient->server_id) : NULL;

            if (guild_player)
                {
                    int64_t samp_calc = guild_player->sampling_rate == -1 ? 48000 : guild_player->sampling_rate;

                    // take account earwax resampling
                    if (guild_player->earwax)
                        samp_calc -= 3900;

                    // (buffer_size / (sampling rate * channel * (bit width(16) / bit per byte(8))) * 1 second in ms) * opus byte_per_ms
                    int64_t add = (int64_t)((double)((float)((float)*send_buffer_length / (samp_calc * 2 * 2) * 1000) * opus_byte_per_ms)
                                            * guild_player->tempo);
                    guild_player->current_track.current_byte += add;
                }
        }

    try
        {
            if (!opus_encoder)
                return 2;

            std::vector<uint16_t> pcmbuf (send_buffer, send_buffer + *send_buffer_length);
            while (!pcmbuf.empty ())
                {
                    uint8_t packet[OPUS_MAX_ENCODE_OUTPUT_SIZE];

                    const auto pbufsiz = pcmbuf.size ();
                    if (pbufsiz < ENCODE_BUFFER_SIZE)
                        {
                            if (debug)
                                {
                                    fprintf (stderr,
                                             "[player::send_audio_"
                                             "routine] Found last chunk of "
                                             "PCM buffer with size: %ld\n",
                                             pbufsiz);
                                }

                            pcmbuf.resize (ENCODE_BUFFER_SIZE);
                        }

                    int len = opus_encode (opus_encoder, (opus_int16 *)pcmbuf.data (), FRAME_SIZE, packet, OPUS_MAX_ENCODE_OUTPUT_SIZE);

                    if (len < 0 || len > OPUS_MAX_ENCODE_OUTPUT_SIZE)
                        {
                            fprintf (stderr,
                                     "[player::send_audio_routine ERROR] "
                                     "opus_encode() returned %d\n",
                                     len);

                            return len;
                        }

                    if (len > 2)
                        {
                            vclient->send_audio_opus (packet, len, FRAME_DURATION);

                            // !TODO: use ogg_stream_t (need to build OpusHead and OpusTags headers manually)
                            // stream_codec::ogg_stream_t s (fd, stream_codec::OGG_STREAM_SUBMIT_OPUS_PACKET);
                        }

                    pcmbuf.erase (pcmbuf.begin (), pcmbuf.begin () + ENCODE_BUFFER_SIZE);
                }
        }
    catch (const dpp::voice_exception &e)
        {
            fprintf (stderr, "[player::send_audio_routine ERROR] %s\n", e.what ());
        }

    *send_buffer_length = 0;

    return 0;
}

#endif // USING_LIBOPUSENC

constexpr const char *msprrfmt = "[Manager::stream ERROR] Processor not ready or exited: %s\n";
constexpr const char *fbsefmt = "[Manager::stream] Final buffer `%s`: %ld %ld\n";
constexpr const char *dssefmt = "[Manager::stream] Done streaming `%s` for %lld milliseconds\n";

void
Manager::stream (const dpp::snowflake &guild_id)
{
    //     auto guild_player = guild_id ? this->get_player (guild_id) : nullptr;
    //     if (!guild_player)
    //         throw 2;
    //
    //     auto vclient = guild_player->get_voice_client ();
    //     if (!vclient)
    //         throw 2;
    //
    //     if (vclient->terminating || !vclient->is_ready ())
    //         throw 1;
    //
    //     // voice_client is ready, mark continuing success
    //     guild_player->tried_continuing = false;
    //
    //     bool debug = get_debug_state ();
    //     MCTrack &track = guild_player->current_track;
    //     std::chrono::high_resolution_clock::time_point start_time;
    //     const std::string ttitle = mctrack::get_title (track);
    //
    //     const std::string &fname = track.filename;
    //     const std::string music_folder_path = get_music_folder_path ();
    //     const std::string file_path = music_folder_path + fname;
    //
    //     // check if we actually have the file
    //     util::fs::ensure_dir (music_folder_path);
    //     FILE *ofile = fopen (file_path.c_str (), "r");
    //     if (!ofile)
    //         throw 2;
    //     fclose (ofile);
    //     ofile = NULL;
    //
    //     // precondition done, spawn processor and play
    //     server::ws::player::publish_playback_info (guild_id);
    //     server::ws::player::publish_fx (guild_id);
    //
    //     const std::string server_id_str = std::to_string (guild_id);
    //     const std::string slave_id = "processor-" + server_id_str + "." + std::to_string (util::get_current_ts ());
    //
    //     std::string cmd = cc::command_options_keys_t.id + '=' + slave_id + ';' + cc::command_options_keys_t.guild_id + '=' +
    //     server_id_str + ';'
    //                       + cc::command_options_keys_t.command + '=' + cc::command_execute_commands_t.create_audio_processor + ';';
    //
    //     if (debug)
    //         cmd += cc::command_options_keys_t.debug + "=1;";
    //
    //     cmd += cc::command_options_keys_t.file_path + '=' + cc::sanitize_command_value (file_path) + ';' +
    //     cc::command_options_keys_t.volume
    //            + '=' + std::to_string (guild_player->volume) + ';';
    //
    //     if (guild_player->fx_is_tempo_active ())
    //         cmd += cc::command_options_keys_t.helper_chain + '='
    //                + cc::sanitize_command_value ("!atempo=" + std::to_string (guild_player->tempo)) + ';';
    //
    //     if (guild_player->fx_is_pitch_active ())
    //         cmd += cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value (get_ffmpeg_pitch_args
    //         (guild_player->pitch))
    //                + ';';
    //
    //     if (guild_player->fx_is_equalizer_active ())
    //         cmd += cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value ("superequalizer=" +
    //         guild_player->equalizer)
    //                + ';';
    //
    //     if (guild_player->fx_is_sampling_rate_active ())
    //         cmd += cc::command_options_keys_t.helper_chain + '='
    //                + cc::sanitize_command_value ("aresample=" + std::to_string (guild_player->sampling_rate)) + ';';
    //
    //     if (guild_player->fx_is_vibrato_active ())
    //         {
    //             std::string v_args
    //                 = get_ffmpeg_vibrato_args (guild_player->fx_has_vibrato_f (), guild_player->fx_has_vibrato_d (), guild_player);
    //
    //             cmd += cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value ("vibrato=" + v_args) + ';';
    //         }
    //
    //     if (guild_player->fx_is_tremolo_active ())
    //         {
    //             std::string v_args
    //                 = get_ffmpeg_tremolo_args (guild_player->fx_has_tremolo_f (), guild_player->fx_has_tremolo_d (), guild_player);
    //
    //             cmd += cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value ("tremolo=" + v_args) + ';';
    //         }
    //
    //     if (guild_player->fx_is_earwax_active ())
    //         cmd += cc::command_options_keys_t.helper_chain + '=' + cc::sanitize_command_value ("earwax") + ';';
    //
    //     track.check_for_seek_to ();
    //     if (!track.seek_to.empty ())
    //         {
    //             cmd += cc::command_options_keys_t.seek + '=' + cc::sanitize_command_value (track.seek_to) + ';';
    //
    //             track.seek_to = "";
    //             server::ws::player::publish_seek (guild_id, track.current_byte / opus_byte_per_ms);
    //             guild_player->reset_first_track_current_byte ();
    //         }
    //
    //     const std::string exit_cmd = cc::get_exit_command (slave_id);
    //     // kill when fail
    //     if (cc::send_command_wr (cmd, exit_cmd, slave_id, 10) != 0)
    //         throw 3;
    //
    //     const std::string fifo_stream_path = audio_processing::get_audio_stream_fifo_path (slave_id);
    //     const std::string fifo_command_path = audio_processing::get_audio_stream_stdin_path (slave_id);
    //     const std::string fifo_notify_path = audio_processing::get_audio_stream_stdout_path (slave_id);
    //
    //     // OPEN FIFOS
    //     int read_fd = open (fifo_stream_path.c_str (), O_RDONLY);
    //     if (read_fd < 0)
    //         {
    //             cc::send_command (exit_cmd);
    //             throw 2;
    //         }
    //
    //     int command_fd = open (fifo_command_path.c_str (), O_WRONLY);
    //     if (command_fd < 0)
    //         {
    //             cc::send_command (exit_cmd);
    //             close (read_fd);
    //             throw 2;
    //         }
    //
    //     int notification_fd = open (fifo_notify_path.c_str (), O_RDONLY);
    //     if (notification_fd < 0)
    //         {
    //             cc::send_command (exit_cmd);
    //             close (read_fd);
    //             close (command_fd);
    //             throw 2;
    //         }
    //
    //     // wait for processor notification
    //     char nbuf[CMD_BUFSIZE + 1];
    //     size_t nread_size = read (notification_fd, nbuf, CMD_BUFSIZE);
    //
    //     bool processor_read_ready = false;
    //     if (nread_size > 0)
    //         {
    //             nbuf[nread_size] = '\0';
    //             if (std::string (nbuf) == "0")
    //                 processor_read_ready = true;
    //         }
    //
    //     handle_effect_chain_change_states_t effect_states = { guild_player, track, command_fd, read_fd, NULL, notification_fd };
    //
    //     if (!processor_read_ready)
    //         {
    //             fprintf (stderr, msprrfmt, slave_id.c_str ());
    //             cc::send_command (exit_cmd);
    //             close (read_fd);
    //             close (command_fd);
    //             close (notification_fd);
    //             throw 2;
    //         }
    //
    //     EffectStatesListing esl (guild_id, &effect_states);
    //
    //     float dpp_audio_buffer_length_second = get_stream_buffer_size ();
    //     int64_t dpp_audio_sleep_on_buffer_threshold_ms = get_stream_sleep_on_buffer_threshold_ms ();
    //
    //     // I LOVE C++!!!
    //     bool running_state, is_stopping;
    //     server::ws::player::publish_play (guild_id);
    //
    //     // using raw pcm need to change ffmpeg output format to s16le!
    //     ssize_t read_size = 0;
    //     ssize_t current_read = 0;
    //     ssize_t total_read = 0;
    //     uint8_t buffer[STREAM_BUFSIZ];
    //
    //     while ((running_state = get_running_state ()) && !(is_stopping = guild_player->stopping)
    //            && ((current_read = read (read_fd, buffer + read_size, STREAM_BUFSIZ - read_size)) > 0))
    //         {
    //             read_size += current_read;
    //             total_read += current_read;
    //
    //             wait_for_ready_event (guild_id);
    //             if ((is_stopping = guild_player->stopping))
    //                 break;
    //
    //             if (read_size != STREAM_BUFSIZ)
    //                 continue;
    //
    //             vclient = guild_player->get_voice_client ();
    //             if (send_audio_routine (vclient, (uint16_t *)buffer, &read_size, false, guild_player->opus_encoder))
    //                 break;
    //
    //             handle_effect_chain_change (effect_states);
    //
    //             float outbuf_duration;
    //
    //             while ((running_state = get_running_state ()) && !wait_for_ready_event (guild_id)
    //                    && (vclient = guild_player->get_voice_client ()) && vclient && !vclient->terminating
    //                    && ((outbuf_duration = vclient->get_secs_remaining ()) > dpp_audio_buffer_length_second))
    //                 {
    //                     handle_effect_chain_change (effect_states);
    //                     std::this_thread::sleep_for (std::chrono::milliseconds (dpp_audio_sleep_on_buffer_threshold_ms));
    //                 }
    //         }
    //
    //     if ((read_size > 0) && running_state && !is_stopping)
    //         {
    //             if (debug)
    //                 fprintf (stderr, fbsefmt, ttitle.c_str (), (total_read += read_size), read_size);
    //
    //             vclient = guild_player->get_voice_client ();
    //             send_audio_routine (vclient, (uint16_t *)buffer, &read_size, true, guild_player->opus_encoder);
    //         }
    //
    // #ifdef USING_LIBOPUSENC
    //     ope_encoder_drain (guild_player->opus_encoder);
    // #endif // USING_LIBOPUSENC
    //
    //     close (read_fd);
    //     read_fd = -1;
    //     close (command_fd);
    //     command_fd = -1;
    //     close (notification_fd);
    //     notification_fd = -1;
    //
    //     if (debug)
    //         std::cerr << "Exiting " << guild_id << '\n';
    //
    //     cc::send_command (exit_cmd);
    //
    //     vclient = guild_player->get_voice_client ();
    //     if (!running_state || is_stopping)
    //         {
    //             // clear voice client buffer
    //             if (vclient)
    //                 vclient->stop_audio ();
    //
    //             server::ws::player::publish_stop (guild_id);
    //         }
    //     else if (!vclient && !guild_player->queue.empty ())
    //         {
    //             // somethins wrong, set last byte so we can continue playback later
    //             guild_player->queue.front ().current_byte = guild_player->current_track.current_byte;
    //
    //             if (debug)
    //                 std::cerr << "[Manager::stream] set current_byte: guild_player->queue.front("
    //                           << mctrack::get_title (guild_player->queue.front ()) << ") current_byte("
    //                           << guild_player->current_track.current_byte << ")\n";
    //         }
    //
    //     auto end_time = std::chrono::high_resolution_clock::now ();
    //     auto done = std::chrono::duration_cast<std::chrono::milliseconds> (end_time - start_time);
    //
    //     if (debug)
    //         fprintf (stderr, dssefmt, ttitle.c_str (), done.count ());
}

class stream_ctx
{
    std::chrono::high_resolution_clock::time_point start_time;
    std::chrono::high_resolution_clock::time_point last_run_time;

    decoder_t dec;

  public:
    dpp::snowflake guild_id;
    bool handled;

  private:
    bool running_state;
    bool is_stopping;

    float dpp_audio_buffer_length_second;
    int64_t dpp_audio_sleep_on_buffer_threshold_ms;

    ssize_t read_size = 0;
    ssize_t current_read = 0;
    ssize_t total_read = 0;
    uint8_t *buffer;
    std::vector<uint16_t> out;

    std::shared_ptr<Player>
    get_guild_player ()
    {
        auto *player_manager = get_player_manager_ptr ();
        return player_manager ? player_manager->get_player (guild_id) : nullptr;
    }

  public:
    stream_ctx (const dpp::snowflake &_guild_id) : guild_id (_guild_id) {}

    int
    init ()
    {
        handled = false;
        auto guild_player = get_guild_player ();
        if (!guild_player)
            throw 2;

        // voice_client is ready, mark continuing success
        guild_player->tried_continuing = false;

        MCTrack &track = guild_player->current_track;
        start_time = std::chrono::high_resolution_clock::now ();
        last_run_time = start_time;

        const std::string &fname = track.filename;
        const std::string music_folder_path = get_music_folder_path ();
        const std::string file_path = music_folder_path + fname;

        // check if we actually have the file
        util::fs::ensure_dir (music_folder_path);
        FILE *ofile = fopen (file_path.c_str (), "r");
        if (!ofile)
            throw 2;
        fclose (ofile);
        ofile = NULL;

        if (!dec.is_valid ())
            throw 2;

        if (dec.open (file_path.c_str ()) < 0)
            throw 2;

        // precondition done, play
        server::ws::player::publish_playback_info (guild_id);
        server::ws::player::publish_fx (guild_id);

        dec.set_filter_descr (guild_player->get_filter_descr ());
        if (dec.init_filters () < 0)
            throw 2;

        track.check_for_seek_to ();
        if (!track.seek_to.empty ())
            {
                const auto ms = util::get_track_progress (track).current_ms;
                dec.seek (ms);
                track.seek_to = "";
                server::ws::player::publish_seek (guild_id, ms);
                guild_player->reset_first_track_current_byte ();
            }

        dpp_audio_buffer_length_second = get_stream_buffer_size ();
        dpp_audio_sleep_on_buffer_threshold_ms = get_stream_sleep_on_buffer_threshold_ms ();

        // I LOVE C++!!!
        running_state = false;
        is_stopping = false;

        // using raw pcm need to change ffmpeg output format to s16le!
        read_size = 0;
        current_read = 0;
        total_read = 0;
        buffer = nullptr;

        server::ws::player::publish_play (guild_id);
        return 0;
    }

    int
    run ()
    {
        auto *player_manager = get_player_manager_ptr ();
        auto guild_player = player_manager ? player_manager->get_player (guild_id) : nullptr;
        if (!guild_player)
            return -1;

        running_state = get_running_state ();
        is_stopping = guild_player->stopping;
        if (!running_state || is_stopping)
            return -1;

        if (player_manager->is_waiting_vc_ready (guild_id))
            return 0;

        auto *vclient = guild_player->get_voice_client ();
        if (!vclient || vclient->terminating)
            return -1;

        const bool debug = get_debug_state ();
        if (debug)
            {
                auto cur_time = std::chrono::high_resolution_clock::now ();

                auto delay_between_run_second = (float)((cur_time - last_run_time).count ()) / 1000000000;
                fprintf (stderr, "[Manager::stream] delay_between_run_second(%f)\n", delay_between_run_second);

                last_run_time = cur_time;
            }

        std::chrono::high_resolution_clock::time_point decode_ts, decode_end, encode_ts, encode_end;

        if (debug)
            decode_ts = std::chrono::high_resolution_clock::now ();
        handle_effect_chain_change ({ guild_player, guild_player->current_track, dec });

        int ret = dec.process_frame (out);
        if (ret == AVERROR_EOF || ret < 0)
            return -1;
        buffer = (uint8_t *)out.data ();
        current_read = out.size () * sizeof (uint16_t);

        read_size += current_read;
        total_read += current_read;

        if (debug)
            {
                decode_end = std::chrono::high_resolution_clock::now ();
                encode_ts = std::chrono::high_resolution_clock::now ();
            }
        if (send_audio_routine (guild_player.get (), buffer, &read_size, false, guild_player->opus_encoder))
            return -1;

        if (debug)
            {
                encode_end = std::chrono::high_resolution_clock::now ();

                auto decode_latency = (decode_end - decode_ts).count ();
                auto encode_latency = (encode_end - encode_ts).count ();
                float total_latency_second = ((float)decode_latency / 1000000000) + ((float)encode_latency / 1000000000);

                fprintf (stderr, "[Manager::stream] decode_latency(%lldns) encode_latency(%lldns) total_latency_second(%f)\n",
                         decode_latency, encode_latency, total_latency_second);
            }

        return 0;
    }

    int
    need_handler ()
    {
        auto *player_manager = get_player_manager_ptr ();
        auto guild_player = player_manager ? player_manager->get_player (guild_id) : nullptr;
        if (!guild_player)
            return -1;

        running_state = get_running_state ();
        if (!running_state)
            return -1;

        if (player_manager->is_waiting_vc_ready (guild_id))
            return 0;

        auto *vclient = guild_player->get_voice_client ();
        if (!vclient || vclient->terminating)
            return -1;

        float outbuf_duration = vclient->get_secs_remaining ();
        if ((outbuf_duration > dpp_audio_buffer_length_second))
            return 0;

        return 1;
    }

    int
    end ()
    {
        auto guild_player = get_guild_player ();
        if (!guild_player)
            return -1;

        const bool debug = get_debug_state ();
        MCTrack &track = guild_player->current_track;
        const std::string ttitle = mctrack::get_title (track);

        if ((read_size > 0) && running_state && !is_stopping)
            {
                if (debug)
                    fprintf (stderr, fbsefmt, ttitle.c_str (), (total_read += read_size), read_size);
            }

#ifdef USING_LIBOPUSENC
        ope_encoder_drain (guild_player->opus_encoder);
#endif // USING_LIBOPUSENC

        auto *vclient = guild_player->get_voice_client ();
        if (!running_state || is_stopping)
            {
                // clear voice client buffer
                if (vclient)
                    vclient->stop_audio ();

                server::ws::player::publish_stop (guild_id);
            }
        else if (!vclient && !guild_player->queue.empty ())
            {
                // somethins wrong, set last byte so we can continue playback later
                guild_player->queue.front ().current_byte = guild_player->current_track.current_byte;

                if (debug)
                    std::cerr << "[Manager::stream] set current_byte: guild_player->queue.front("
                              << mctrack::get_title (guild_player->queue.front ()) << ") current_byte("
                              << guild_player->current_track.current_byte << ")\n";
            }

        auto end_time = std::chrono::high_resolution_clock::now ();
        auto done = std::chrono::duration_cast<std::chrono::milliseconds> (end_time - start_time);

        if (debug)
            fprintf (stderr, dssefmt, ttitle.c_str (), done.count ());

        guild_player->done_streaming ();
        return 0;
    }
};

static std::vector<stream_ctx> stream_ctxs;
static std::mutex stream_ctxs_m;
static std::condition_variable stream_ctxs_cv;

void
on_clear_wait_vc_ready ()
{
    stream_ctxs_cv.notify_one ();
}

void
shutdown ()
{
    stream_ctxs_cv.notify_all ();
}

static void
erase_ctx_unlocked (const dpp::snowflake &guild_id)
{
    auto i = stream_ctxs.begin ();
    while (i != stream_ctxs.end ())
        {
            if (i->guild_id == guild_id)
                {
                    stream_ctxs.erase (i);
                    break;
                }

            i++;
        }
}

static void
erase_ctx (const dpp::snowflake &guild_id)
{
    std::lock_guard lk (stream_ctxs_m);
    erase_ctx_unlocked (guild_id);
}

static void
run_stream_thread ()
{
    stream_ctx *ctx = nullptr;
    while (get_running_state ())
        {
            ctx = nullptr;
            {
                std::unique_lock lk (stream_ctxs_m);
                for (auto &c : stream_ctxs)
                    {
                        if (c.handled || !c.need_handler ())
                            continue;
                        c.handled = true;
                        ctx = &c;
                        break;
                    }

                if (!ctx)
                    {
                        // wait for stream_ctxs
                        stream_ctxs_cv.wait (lk);
                        continue;
                    }
            }

            while (ctx && ctx->need_handler ())
                {
                    if (!ctx->run ())
                        continue;
                    ctx->end ();

                    auto *player_manager = get_player_manager_ptr ();
                    auto guild_player = player_manager ? player_manager->get_player (ctx->guild_id) : nullptr;

                    erase_ctx (ctx->guild_id);
                    ctx = nullptr;

                    auto *vclient = guild_player ? guild_player->get_voice_client () : nullptr;
                    if (vclient && !vclient->terminating)
                        vclient->insert_marker ("e");
                }

            if (!ctx)
                continue;

            std::lock_guard lk (stream_ctxs_m);
            ctx->handled = false;
        }
}

static int stream_thread_count = 0;

void
spawn_stream_thread (int count)
{
    if (stream_thread_count != 0)
        return;
    stream_thread_count = count;

    fprintf (stderr, "[player::spawn_stream_thread] Spawning %d stream thread\n", count);
    for (int i = 0; i < count; i++)
        {
            std::thread t (
                [] ()
                    {
                        thread_manager::DoneSetter tmds;
                        run_stream_thread ();
                    });
            thread_manager::dispatch (t);
        }
}

void
check_stream_contexts ()
{
    std::unique_lock lk (stream_ctxs_m);
    int notified = 0;
    for (auto &c : stream_ctxs)
        {
            if (c.handled || !c.need_handler ())
                continue;

            stream_ctxs_cv.notify_one ();
            notified++;
            if (notified >= stream_thread_count)
                break;
        }
}

void
Manager::submit_stream_ctx (const dpp::snowflake &guild_id)
{
    {
        std::lock_guard lk (stream_ctxs_m);
        auto i = stream_ctxs.begin ();
        while (i != stream_ctxs.end ())
            {
                if (i->guild_id == guild_id)
                    throw 3;

                i++;
            }
        auto &nctx = stream_ctxs.emplace_back (guild_id);
        try
            {
                nctx.init ();
            }
        catch (int e)
            {
                erase_ctx_unlocked (nctx.guild_id);
                throw e;
            }
    }
    stream_ctxs_cv.notify_one ();
}

void
Manager::stream_noslave (const dpp::snowflake &guild_id)
{
    //     auto guild_player = guild_id ? this->get_player (guild_id) : nullptr;
    //     if (!guild_player)
    //         throw 2;
    //
    //     auto vclient = guild_player->get_voice_client ();
    //     if (!vclient)
    //         throw 2;
    //
    //     if (vclient->terminating || !vclient->is_ready ())
    //         throw 1;
    //
    //     // voice_client is ready, mark continuing success
    //     guild_player->tried_continuing = false;
    //
    //     bool debug = get_debug_state ();
    //     MCTrack &track = guild_player->current_track;
    //     std::chrono::high_resolution_clock::time_point start_time;
    //     const std::string ttitle = mctrack::get_title (track);
    //
    //     const std::string &fname = track.filename;
    //     const std::string music_folder_path = get_music_folder_path ();
    //     const std::string file_path = music_folder_path + fname;
    //
    //     // check if we actually have the file
    //     util::fs::ensure_dir (music_folder_path);
    //     FILE *ofile = fopen (file_path.c_str (), "r");
    //     if (!ofile)
    //         throw 2;
    //     fclose (ofile);
    //     ofile = NULL;
    //
    //     decoder_t dec;
    //     if (!dec.is_valid ())
    //         throw 2;
    //
    //     if (dec.open (file_path.c_str ()) < 0)
    //         throw 2;
    //
    //     // precondition done, play
    //     server::ws::player::publish_playback_info (guild_id);
    //     server::ws::player::publish_fx (guild_id);
    //
    //     dec.set_filter_descr (guild_player->get_filter_descr ());
    //     if (dec.init_filters () < 0)
    //         throw 2;
    //
    //     track.check_for_seek_to ();
    //     if (!track.seek_to.empty ())
    //         {
    //             const auto ms = util::get_track_progress (track).current_ms;
    //             dec.seek (ms);
    //             track.seek_to = "";
    //             server::ws::player::publish_seek (guild_id, ms);
    //             guild_player->reset_first_track_current_byte ();
    //         }
    //
    //     handle_effect_chain_change_states_t effect_states = { guild_player, track, dec };
    //
    //     EffectStatesListing esl (guild_id, &effect_states);
    //
    //     float dpp_audio_buffer_length_second = get_stream_buffer_size ();
    //     int64_t dpp_audio_sleep_on_buffer_threshold_ms = get_stream_sleep_on_buffer_threshold_ms ();
    //
    //     // I LOVE C++!!!
    //     bool running_state, is_stopping;
    //     server::ws::player::publish_play (guild_id);
    //
    //     // using raw pcm need to change ffmpeg output format to s16le!
    //     ssize_t read_size = 0;
    //     ssize_t current_read = 0;
    //     ssize_t total_read = 0;
    //     uint8_t *buffer;
    //
    //     std::vector<uint16_t> out;
    //     while ((running_state = get_running_state ()) && !(is_stopping = guild_player->stopping))
    //         {
    //             auto decode_ts = std::chrono::high_resolution_clock::now ();
    //             int ret = dec.process_frame (out);
    //             if (ret == AVERROR_EOF || ret < 0)
    //                 break;
    //             buffer = (uint8_t *)out.data ();
    //             current_read = out.size () * sizeof (uint16_t);
    //
    //             read_size += current_read;
    //             total_read += current_read;
    //
    //             auto decode_end = std::chrono::high_resolution_clock::now ();
    //
    //             wait_for_ready_event (guild_id);
    //
    //             if ((is_stopping = guild_player->stopping))
    //                 break;
    //
    //             auto encode_ts = std::chrono::high_resolution_clock::now ();
    //             vclient = guild_player->get_voice_client ();
    //             if (send_audio_routine (vclient, buffer, &read_size, false, guild_player->opus_encoder))
    //                 break;
    //
    //             handle_effect_chain_change (effect_states);
    //             auto encode_end = std::chrono::high_resolution_clock::now ();
    //
    //             float outbuf_duration;
    //             auto decode_latency = (decode_end - decode_ts).count ();
    //             auto encode_latency = (encode_end - encode_ts).count ();
    //             float total_latency_second = ((float)decode_latency / 1000000000) + ((float)encode_latency / 1000000000);
    //             if (debug)
    //                 fprintf (stderr, "[Manager::stream] decode_latency(%lldns) encode_latency(%lldns) total_latency_second(%f)\n",
    //                          decode_latency, encode_latency, total_latency_second);
    //
    //             while ((running_state = get_running_state ()) && !wait_for_ready_event (guild_id)
    //                    && (vclient = guild_player->get_voice_client ()) && vclient && !vclient->terminating
    //                    && ((outbuf_duration = vclient->get_secs_remaining ()) > (dpp_audio_buffer_length_second + total_latency_second)))
    //                 {
    //                     handle_effect_chain_change (effect_states);
    //                     std::this_thread::sleep_for (std::chrono::milliseconds (dpp_audio_sleep_on_buffer_threshold_ms));
    //                 }
    //         }
    //
    //     if ((read_size > 0) && running_state && !is_stopping)
    //         {
    //             if (debug)
    //                 fprintf (stderr, fbsefmt, ttitle.c_str (), (total_read += read_size), read_size);
    //         }
    //
    // #ifdef USING_LIBOPUSENC
    //     ope_encoder_drain (guild_player->opus_encoder);
    // #endif // USING_LIBOPUSENC
    //
    //     vclient = guild_player->get_voice_client ();
    //     if (!running_state || is_stopping)
    //         {
    //             // clear voice client buffer
    //             if (vclient)
    //                 vclient->stop_audio ();
    //
    //             server::ws::player::publish_stop (guild_id);
    //         }
    //     else if (!vclient && !guild_player->queue.empty ())
    //         {
    //             // somethins wrong, set last byte so we can continue playback later
    //             guild_player->queue.front ().current_byte = guild_player->current_track.current_byte;
    //
    //             if (debug)
    //                 std::cerr << "[Manager::stream] set current_byte: guild_player->queue.front("
    //                           << mctrack::get_title (guild_player->queue.front ()) << ") current_byte("
    //                           << guild_player->current_track.current_byte << ")\n";
    //         }
    //
    //     auto end_time = std::chrono::high_resolution_clock::now ();
    //     auto done = std::chrono::duration_cast<std::chrono::milliseconds> (end_time - start_time);
    //
    //     if (debug)
    //         fprintf (stderr, dssefmt, ttitle.c_str (), done.count ());
}

void
Manager::set_processor_state (std::string &server_id_str, processor_state_t state)
{
    std::lock_guard lk (this->as_m);

    this->processor_states[server_id_str] = state;
}

processor_state_t
Manager::get_processor_state (std::string &server_id_str)
{
    std::lock_guard lk (this->as_m);
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
