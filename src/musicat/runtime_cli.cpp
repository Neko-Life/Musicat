#include "musicat/runtime_cli.h"
#include "musicat/mctrack.h"
#include "musicat/musicat.h"
#include "musicat/player.h"
#include "musicat/thread_manager.h"
#include "musicat/util.h"
#include <cstdint>
#include <sys/poll.h>

using cmd_args_t = std::vector<std::string>;

struct command_entry_t
{
    const char *name;
    const char *alias;
    const char *description;
    int (*const handler) (const cmd_args_t &args);
};

////////////////////////////////////////////////////////////////////////////////

static void
_print_pad (size_t len)
{
    if ((long long)len < 0LL)
        return;

    for (size_t i = 0; i < len; i++)
        {
            fprintf (stderr, " ");
        }
}

static bool
_cmd_end (const command_entry_t &cmd)
{
    return !cmd.name && !cmd.alias && !cmd.description && !cmd.handler;
}

////////////////////////////////////////////////////////////////////////////////

namespace musicat::runtime_cli
{
static bool attached = false;
const command_entry_t *commands_ptr = NULL;
std::mutex ns_mutex; // EXTERN_VARIABLE

static size_t padding_command = 0;
static size_t padding_alias = 0;

////////////////////////////////////////////////////////////////////////////////

static int
help_cmd (const cmd_args_t &args)
{
    std::lock_guard lk (ns_mutex);
    if (!commands_ptr)
        {
            fprintf (stderr, "[runtime_cli::help_cmd ERROR] Commands ptr null\n");
            return 1;
        }

    fprintf (stderr, "Usage: [command] [args] <ENTER>\n\n");

    size_t i = 0;
    while (true)
        {
            auto cmd = commands_ptr[i++];

            bool end = _cmd_end (cmd);
            if (end)
                break;

            size_t name_len = cmd.name ? strlen (cmd.name) : 0;
            size_t alias_len = cmd.alias ? strlen (cmd.alias) : 0;

            // print only new line if there's dummy handler
            if (!name_len && !alias_len && !cmd.description)
                {
                    fprintf (stderr, "\n");
                    continue;
                }

            if (name_len)
                fprintf (stderr, "%s", cmd.name);

            _print_pad (padding_command - name_len);

            fprintf (stderr, (name_len || alias_len) ? ":" : " ");

            const size_t pad_a = padding_alias - alias_len;

            _print_pad (pad_a / 2);

            if (alias_len)
                fprintf (stderr, "%s", cmd.alias);

            _print_pad ((size_t)ceil ((double)pad_a / (double)2.0));

            if (cmd.description)
                fprintf (stderr, "%s %s\n", (name_len || alias_len) ? ":" : " ", cmd.description);
        }

    fprintf (stderr, "\n");

    return 0;
}

static int
debug_cmd (const cmd_args_t &args)
{
    set_debug_state (!get_debug_state ());
    return 0;
}

static int
clear_cmd (const cmd_args_t &args)
{
    system ("clear");
    return 0;
}

static int
shutdown_cmd (const cmd_args_t &args)
{
    fprintf (stderr, "Shutting down...\n");
    set_running_state (false);
    return 0;
}

static int
list_effect_states (const cmd_args_t &args)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    for (auto &[s, guild_player] : player_manager->players)
        {
            auto gid = guild_player->guild_id;

            auto g = dpp::find_guild (gid);
            std::string gstr = g ? g->name : "[not_found]";

            std::cerr << gstr << " (" << gid << "):\nTrack: " << mctrack::get_title (guild_player->current_track) << '\n'
                      << "FX: " << guild_player->get_filter_descr () << "\n==========\n";
        }

    return 0;
}

static int
effect_states_send_command (const cmd_args_t &args)
{

    return 0;
}

static int
join_guild (dpp::guild *g, dpp::user *sha_user)
{
    auto player_manager = get_player_manager_ptr ();
    auto sha_id = get_sha_id ();
    auto guild_id = g->id;

    {
        // get voice state of sha_id in guild_id
        // check if we're connected in this guild
        auto m = get_voice_from_gid (guild_id, sha_id);
        if (m.first)
            {
                fprintf (stderr, "Already in voice/stage channel `%s` (%ld) in guild `%s` (%ld)\n", m.first->name.c_str (),
                         (uint64_t)m.first->id, g->name.c_str (), (uint64_t)guild_id);
                return 0;
            }
    }

    dpp::channel *join_channel = nullptr;
    for (auto &fc : g->channels)
        {
            auto *gc = dpp::find_channel (fc);
            if (!gc || (!gc->is_voice_channel () && !gc->is_stage_channel ()))
                continue;

            std::vector<uint64_t> need_perms = { dpp::p_view_channel, dpp::p_connect };
            if (gc->is_stage_channel ())
                need_perms.push_back (dpp::p_request_to_speak);
            else
                need_perms.push_back (dpp::p_speak);

            if (!has_permissions (g, sha_user, gc, need_perms))
                continue;

            join_channel = gc;
            break;
        }

    if (!join_channel)
        {
            fprintf (stderr, "No joinable voice/stage channel in guild `%s` (%ld)\n", g->name.c_str (), (uint64_t)guild_id);
            return 0;
        }

    fprintf (stderr, "Joining voice/stage channel `%s` (%ld) in guild `%s` (%ld)\n", join_channel->name.c_str (),
             (uint64_t)join_channel->id, g->name.c_str (), (uint64_t)guild_id);

    player_manager->full_reconnect (player_manager->get_client (g->shard_id), guild_id, dpp::snowflake (0), join_channel->id);
    player_manager->wait_for_vc_ready (guild_id);
    return 0;
}

static int
join_all (const cmd_args_t &args)
{
    // for each guild, if not already in vc, find joinable vc and join
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto sha_id = get_sha_id ();
    auto &sha_user = player_manager->cluster->me;

    std::vector<dpp::guild *> vgc;
    {
        auto *c = dpp::get_guild_cache ();
        std::shared_lock lk (c->get_mutex ());
        std::unordered_map<dpp::snowflake, dpp::guild *> &gc = c->get_container ();
        for (auto &[_id, g] : gc)
            {
                vgc.push_back (g);
            }
    }

    for (auto *g : vgc)
        {
            join_guild (g, &sha_user);
        }

    return 0;
}

static int
join (const cmd_args_t &args)
{
    if (args.empty ())
        {
            fprintf (stderr, "Provide <guild_id>\n");
            return 0;
        }
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto sha_id = get_sha_id ();
    auto &sha_user = player_manager->cluster->me;

    dpp::snowflake guild_id{ args.at (0) };
    if (guild_id.empty ())
        {
            fprintf (stderr, "Invalid <guild_id>\n");
            return 0;
        }

    auto *g = dpp::find_guild (guild_id);
    if (!g)
        {
            fprintf (stderr, "Guild (%ld) not found\n", (uint64_t)guild_id);
            return 0;
        }

    return join_guild (g, &sha_user);
}

static std::vector<player::gat_t>
get_enqueue_get ()
{
    const std::vector<player::gat_t> get = player::get_available_tracks ();
    if (get.empty ())
        {
            fprintf (stderr, "No playable track exists\n");
            return {};
        }
    fprintf (stderr, "Track count: %ld\n", get.size ());
    return get;
}

static int
enqueue_random_track_in_guild (dpp::guild *g, const std::vector<player::gat_t> &get = {})
{
    auto sha_id = get_sha_id ();
    auto *player_manager = get_player_manager_ptr ();
    auto guild_id = g->id;

    auto guild_player = player_manager->create_player (guild_id);
    guild_player->set_shard (g->shard_id);

    // skip if already playing
    if (guild_player->processing_audio)
        {
            fprintf (stderr, "Already playing in guild `%s` (%ld), skipping\n", g->name.c_str (), (uint64_t)guild_id);
            return 0;
        }

    // get voice_client of this guild
    // check if we're connected in this guild
    auto *vclient = guild_player->get_voice_client ();
    if (!vclient)
        {
            fprintf (stderr, "Not in any voice/stage channel in guild `%s` (%ld), skipping\n", g->name.c_str (), (uint64_t)guild_id);
            return 0;
        }

    if (!vclient->is_ready ())
        {
            fprintf (stderr, "Voice client in voice/stage channel (%ld) in guild `%s` (%ld) is not ready yet, skipping\n",
                     (uint64_t)vclient->channel_id, g->name.c_str (), (uint64_t)guild_id);
            return 0;
        }

    // get random track and play it
    const auto &atrack = util::rand_item (get);
    fprintf (stderr, "Playing `%s` in guild `%s` (%ld)\n", atrack.name.c_str (), g->name.c_str (), (uint64_t)guild_id);
    player::add_track (false, guild_id, atrack.name, 0, true, NULL, 0, sha_id, false, guild_player->shard_id);

    return 0;
}

static int
enqueue_random_track_in_guild (const dpp::snowflake &guild_id, const std::vector<player::gat_t> &get = {})
{
    auto *g = dpp::find_guild (guild_id);
    if (!g)
        {
            fprintf (stderr, "Guild (%ld) not found\n", (uint64_t)guild_id);
            return 0;
        }

    return enqueue_random_track_in_guild (g, get);
}

static int
enqueue_rand (const cmd_args_t &args)
{
    // for guild args[0], if already in vc AND not playing anything, find random track and play it
    if (args.empty ())
        {
            fprintf (stderr, "Provide <guild_id>\n");
            return 0;
        }
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    dpp::snowflake guild_id{ args.at (0) };
    if (guild_id.empty ())
        {
            fprintf (stderr, "Invalid <guild_id>\n");
            return 0;
        }

    return enqueue_random_track_in_guild (guild_id, get_enqueue_get ());
}

static int
enqueue_all (const cmd_args_t &args)
{
    // for each guild, if already in vc AND not playing anything, find random track and play it
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    const std::vector<player::gat_t> get = get_enqueue_get ();
    if (get.empty ())
        return 0;

    auto *c = dpp::get_guild_cache ();
    std::shared_lock lk (c->get_mutex ());
    std::unordered_map<dpp::snowflake, dpp::guild *> &gc = c->get_container ();
    for (auto &[_id, g] : gc)
        enqueue_random_track_in_guild (g, get);

    return 0;
}

static int
play_all (const cmd_args_t &args)
{
    auto *player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    for (auto &[s, guild_player] : player_manager->players)
        {
            auto guild_id = guild_player->guild_id;
            auto *voiceclient = guild_player->get_voice_client ();

            if (!voiceclient)
                {
                    fprintf (stderr, "Not in any voice/stage channel in guild (%ld), skipping\n", (uint64_t)guild_id);
                    continue;
                }

            if (!voiceclient->is_ready ())
                {
                    fprintf (stderr, "Voice client in voice/stage channel (%ld) in guild (%ld) is not ready yet, skipping\n",
                             (uint64_t)voiceclient->channel_id, (uint64_t)guild_id);
                    continue;
                }

            if ((!voiceclient->is_paused () && !voiceclient->is_playing ()) || voiceclient->get_secs_remaining () < 0.05f)
                {
                    fprintf (stderr, "Starting voice client in voice/stage channel (%ld) in guild (%ld)...\n",
                             (uint64_t)voiceclient->channel_id, (uint64_t)guild_id);

                    voiceclient->insert_marker ("s");
                }
            else
                fprintf (stderr, "Voice client in voice/stage channel (%ld) in guild (%ld) is paused or already playing, skipping\n",
                         (uint64_t)voiceclient->channel_id, (uint64_t)guild_id);
        }

    return 0;
}

static int
play (const cmd_args_t &args)
{
    if (args.empty ())
        {
            fprintf (stderr, "Provide <guild_id>\n");
            return 0;
        }
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    dpp::snowflake guild_id{ args.at (0) };
    if (guild_id.empty ())
        {
            fprintf (stderr, "Invalid <guild_id>\n");
            return 0;
        }

    auto *g = dpp::find_guild (guild_id);
    if (!g)
        {
            fprintf (stderr, "Guild (%ld) not found\n", (uint64_t)guild_id);
            return 0;
        }

    auto guild_player = player_manager->create_player (guild_id);
    guild_player->set_shard (g->shard_id);

    auto *voiceclient = guild_player->get_voice_client ();

    if (!voiceclient)
        {
            fprintf (stderr, "Not in any voice/stage channel in guild (%ld), skipping\n", (uint64_t)guild_id);
            return 0;
        }

    if (!voiceclient->is_ready ())
        {
            fprintf (stderr, "Voice client in voice/stage channel (%ld) in guild (%ld) is not ready yet, skipping\n",
                     (uint64_t)voiceclient->channel_id, (uint64_t)guild_id);
            return 0;
        }

    if ((!voiceclient->is_paused () && !voiceclient->is_playing ()) || voiceclient->get_secs_remaining () < 0.05f)
        {
            fprintf (stderr, "Starting voice client in voice/stage channel (%ld) in guild (%ld)...\n", (uint64_t)voiceclient->channel_id,
                     (uint64_t)guild_id);

            voiceclient->insert_marker ("s");
        }
    else
        fprintf (stderr, "Voice client in voice/stage channel (%ld) in guild (%ld) is paused or already playing, skipping\n",
                 (uint64_t)voiceclient->channel_id, (uint64_t)guild_id);

    return 0;
}

static int
leave_all (const cmd_args_t &args)
{
    // for each guild, leave vc
    auto player_manager = get_player_manager_ptr ();
    if (!player_manager)
        return -1;

    auto sha_id = get_sha_id ();

    std::vector<dpp::guild *> vgc;
    {
        auto *c = dpp::get_guild_cache ();
        std::shared_lock lk (c->get_mutex ());
        std::unordered_map<dpp::snowflake, dpp::guild *> &gc = c->get_container ();
        for (auto &[_id, g] : gc)
            {
                vgc.push_back (g);
            }
    }

    for (auto *g : vgc)
        {
            auto guild_id = g->id;
            auto *client = player_manager->get_client (g->shard_id);
            if (!client)
                continue;

            // check if we're connected in this guild
            auto *voiceconn = client->get_voice (guild_id);
            if (!voiceconn)
                {
                    fprintf (stderr, "Not connected to any voice/stage channel in guild `%s` (%ld), skipping\n", g->name.c_str (),
                             (uint64_t)guild_id);
                    continue;
                }

            fprintf (stderr, "Leaving voice/stage channel (%ld) in guild `%s` (%ld)\n", (uint64_t)voiceconn->channel_id, g->name.c_str (),
                     (uint64_t)guild_id);

            player_manager->set_disconnecting (guild_id, 0);
            player_manager->disconnect_voice (client, guild_id);
            player_manager->wait_for_disconnecting (guild_id);
        }

    return 0;
}

static int
toggle_play_bypass_listener (const cmd_args_t &args)
{
    bool new_val = !get_play_bypass_listener ();
    set_play_bypass_listener (new_val);

    fprintf (stderr, "New value: %s\n", new_val ? "true" : "false");

    return 0;
}

// !TODO: more cmd? maybe stats/utility

////////////////////////////////////////////////////////////////////////////////

inline constexpr command_entry_t
create_command_entry (const char *name, const char *alias, const char *description, int (*const handler) (const cmd_args_t &args))
{
    return { name, alias, description, handler };
}

// clang-format off
/**
 * Commands list, name alias description handler
 *
 * description only means it's the continuation of above's description
 * handler only means to print new blank line
 *
 * !Always check and adjust aliases to not clash with one another, commands are
 * matched using `starts_with`
 *
 */
inline constexpr const command_entry_t commands[] = {
    { "command",             "alias",  "description",                                                               NULL                        },
    { "help",                "-h",     "Print this message",                                                        help_cmd                    },
    { "debug",               "-d",     "Toggle debug mode",                                                         debug_cmd                   },
    { NULL,                  NULL,     "Debug mode prints everything for debugging purpose",                        NULL                        },
    { "clear",               "-c",     "Clear console",                                                             clear_cmd                   },
    { "shutdown",            NULL,     "Shutdown Musicat",                                                          shutdown_cmd                },
    { "list effect states",  "-ls es", "List currently active effect states",                                       list_effect_states          },
    { "join all",            "-ja",    "Join to random stage/voice channel for every guild (for testing purpose)",  join_all                    },
    { "join guild",          "-jg",    "Join to random stage/voice channel in <guild_id> (for testing purpose)",    join                        },
    { "enqueue all rand",    "-ear",   "Try to enqueue random track for every voice session (for testing purpose)", enqueue_all                 },
    { "enqueue rand",        "-er",    "Try to enqueue random track in guild <guild_id> (for testing purpose)",     enqueue_rand                },
    { "play all",            "-pa",    "Try to start playing for every voice session (for testing purpose)",        play_all                    },
    { "play guild",          "-pg",    "Try to start playing for <guild_id> voice session (for testing purpose)",   play                        },
    { "leave all",           "-la",    "Leave all stage/voice channel for every guild (for testing purpose)",       leave_all                   },
    { "toggle has_listener", "-thl",   "Toggle play_bypass_listener (for testing purpose)",                         toggle_play_bypass_listener },
    { NULL,                  NULL,     NULL,                                                                        NULL                        },
};
// clang-format on

////////////////////////////////////////////////////////////////////////////////

bool
get_attached_state ()
{
    std::lock_guard lk (ns_mutex);
    return attached;
}

bool
set_attached_state (bool s)
{
    std::lock_guard lk (ns_mutex);
    attached = s;
    return attached;
}

/*
'   inter   com iah h   '
'inter  com iah h    '
'inter  com iah h'
'   inter com iah h'
*/

cmd_args_t
parse_args_str (const std::string &args_str)
{
    cmd_args_t ret = {};

    size_t max_i = args_str.length ();

    // return early when no arg length
    if (!max_i)
        return ret;

    // start index
    size_t s_i = 0;
    // valid char found
    bool f = false;
    for (size_t i = 0; i < args_str.length (); i++)
        {
            char c = args_str[i];

            // valid char
            if (c != ' ')
                {
                    // never had found valid char
                    if (!f)
                        {
                            // set current index as the first valid char
                            s_i = i;
                            // set found valid char
                            f = true;
                        }

                    continue;
                }

            // found space

            // never got valid char, do nothing
            if (!f)
                continue;

            // first valid char found
            // push the word
            ret.push_back (args_str.substr (s_i, i - s_i));
            // reset found state
            f = false;
        }

    // end of string
    // check for valid char found
    if (f)
        {
            // push word at the end of string
            ret.push_back (args_str.substr (s_i));
            // reset found state
            // f = false;
        }

    // debug
    /*
    std::cerr << "args: ";
    for (const auto &s : ret)
        {
            std::cerr << '`' << s << "` ";
        }
    std::cerr << "\n";
    */

    return ret;
}

void
handle_command (const std::string &cmd_str)
{
    for (const command_entry_t &command : commands)
        {
            bool end = _cmd_end (command);
            if (end)
                break;

            if (!command.handler)
                continue;

            bool no_name = !command.name;
            bool no_alias = !command.alias;

            if (no_name && no_alias)
                {
                    continue;
                }

            bool match = false, is_alias = false;

            if (!no_alias)
                {
                    match = cmd_str.find (command.alias) == 0;
                    if (match)
                        {
                            is_alias = true;
                        }
                }

            if (!match && !no_name)
                match = cmd_str.find (command.name) == 0;

            if (!match)
                continue;

            std::string args_str = cmd_str.substr (is_alias ? strlen (command.alias) : strlen (command.name));

            int status = command.handler (parse_args_str (args_str));

            if (status != 0)
                {
                    fprintf (stderr, "[%s] Status (%d)\n", cmd_str.c_str (), status);
                }
        }
}

////////////////////////////////////////////////////////////////////////////////

class AttachedReset
{
  public:
    ~AttachedReset ()
    {
        std::lock_guard lk (ns_mutex);
        attached = false;
    }
};

void
load_commands ()
{
    commands_ptr = commands;

    for (const command_entry_t &cmd : commands)
        {
            bool end = _cmd_end (cmd);
            if (end)
                break;

            size_t len_command = (cmd.name ? strlen (cmd.name) : 0) + 1U;
            size_t len_alias = (cmd.alias ? strlen (cmd.alias) : 0) + 2U;

            if (len_command > padding_command)
                padding_command = len_command;

            if (len_alias > padding_alias)
                padding_alias = len_alias;
        }
}

int
attach_listener ()
{
    std::lock_guard lk (ns_mutex);

    if (attached == true)
        {
            fprintf (stderr, "[runtime_cli::attach_listener ERROR] "
                             "Listener already attached!\n");

            return 1;
        }
    attached = true;
    load_commands ();

    fprintf (stderr, "[INFO] Enter `-d` to toggle debug mode\n");

    std::thread stdin_listener (
        [] ()
            {
                thread_manager::DoneSetter tmds;
                dpp::utility::set_thread_name ("mc/runtime_cli");
                AttachedReset ar;

                struct pollfd stdinpfds[1];
                stdinpfds[0].events = POLLIN;
                stdinpfds[0].fd = STDIN_FILENO;

                while (get_running_state ())
                    {
                        std::string cmd_str;

                        // poll for 2 seconds every iteration
                        const int read_has_event = poll (stdinpfds, 1, 2000);

                        std::string codes = "";
                        if (stdinpfds[0].revents & POLLERR)
                            {
                                codes += "POLLERR";
                            }

                        if (stdinpfds[0].revents & POLLHUP)
                            {
                                codes += " POLLHUP";
                            }

                        if (stdinpfds[0].revents & POLLNVAL)
                            {
                                codes += " POLLNVAL";
                            }

                        if (!codes.empty ())
                            {
                                fprintf (stderr, "[runtime_cli::stdin_listener ERROR] %s ", codes.c_str ());

                                perror ("");
                                fprintf (stderr, "Aborting thread...\n");
                                break;
                            }

                        bool read_ready = (read_has_event > 0) && (stdinpfds[0].revents & POLLIN);

                        if (read_ready)
                            std::getline (std::cin, cmd_str);
                        else
                            continue;

                        if (cmd_str.empty ())
                            continue;

                        handle_command (cmd_str);
                    }
            });

    thread_manager::dispatch (stdin_listener);

    return 0;
}

} // musicat::runtime_cli
