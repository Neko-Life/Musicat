#include "musicat/cmds/owner.h"
#include "musicat/musicat.h"
#include <regex>

namespace musicat::command::owner::set_presence
{
std::map<uint32_t, dpp::presence> active_presences;
// must lock this whenever having pointer and changing state
std::mutex active_presences_m;

dpp::presence *
get_current_presence (uint32_t cluster_id)
{
    auto i = active_presences.find (cluster_id);

    if (i == active_presences.end ())
        return nullptr;

    return &i->second;
}

int
set_current_presence (uint32_t cluster_id, const dpp::presence &presence)
{
    active_presences.insert_or_assign (cluster_id, presence);
    return 0;
}

void
setup_subcommand (dpp::slashcommand &slash)
{
    dpp::command_option subcmd (dpp::co_sub_command, "set_presence",
                                "Set Musicat presence/status");

    // description
    subcmd.add_option (dpp::command_option (dpp::co_string, "description",
                                            "Presence description"));

    // status options
    subcmd.add_option (
        dpp::command_option (dpp::co_integer, "status", "Presence status")
            .add_choice (dpp::command_option_choice ("Offline", 0))
            .add_choice (dpp::command_option_choice ("Online", 1))
            .add_choice (dpp::command_option_choice ("DND", 2))
            .add_choice (dpp::command_option_choice ("Idle", 3))
            .add_choice (dpp::command_option_choice ("Invisible", 4)));

    // type options
    subcmd.add_option (
        dpp::command_option (dpp::co_integer, "type", "Presence type")
            .add_choice (dpp::command_option_choice ("Game", 0))
            .add_choice (dpp::command_option_choice ("Streaming", 1))
            .add_choice (dpp::command_option_choice ("Listening", 2))
            .add_choice (dpp::command_option_choice ("Watching", 3))
            .add_choice (dpp::command_option_choice ("Custom", 4))
            .add_choice (dpp::command_option_choice ("Competing", 5)));

    slash.add_option (subcmd);
}

std::string
get_guild_count (const dpp::slashcommand_t &event)
{
    if (!event.from())
        return "ERR";

    return std::to_string (event.from()->get_guild_count ());
}

constexpr
    std::pair<const char *, std::string (*) (const dpp::slashcommand_t &)>
        variables[] = { { "GUILD_COUNT", &get_guild_count }, { NULL, NULL } };

void
slash_run (const dpp::slashcommand_t &event)
{
    dpp::cluster *client = event.from()->creator;

    std::lock_guard lk (active_presences_m);
    dpp::presence *active_presence = get_current_presence (client->cluster_id);

    int64_t new_status = active_presence ? active_presence->status ()
                                         : dpp::presence_status::ps_online;

    int64_t new_type = active_presence
                           ? active_presence->activities.at (0).type
                           : dpp::activity_type::at_watching;

    get_inter_param (event, "status", &new_status);
    get_inter_param (event, "type", &new_type);

    std::string desc
        = active_presence ? active_presence->activities.at (0).name : "";

    get_inter_param (event, "description", &desc);

    // replace variables
    std::regex variable_regex ("\\\\?\\$([A-Z_]+)");

    auto vars_begin
        = std::sregex_iterator (desc.begin (), desc.end (), variable_regex);
    auto vars_end = std::sregex_iterator ();

    std::string final_desc = "";

    if (vars_begin == vars_end)
        final_desc = desc;
    else
        for (std::sregex_iterator i = vars_begin; i != vars_end; ++i)
            {
                std::smatch variable_match = *i;

                std::string match_str = variable_match.str ();
                final_desc += variable_match.prefix ();

                std::ssub_match sub_match = variable_match[1];
                std::string var_name = sub_match.str ();

                std::string (*handler) (const dpp::slashcommand_t &) = NULL;
                bool escaped = match_str[0] == '\\';

                if (!escaped)
                    for (const auto &v : variables)
                        {
                            if (!v.first)
                                break;

                            if (var_name == v.first)
                                {
                                    handler = v.second;
                                    break;
                                }
                        }

                if (handler)
                    final_desc += handler (event);
                else
                    final_desc += (escaped && match_str.length () > 1)
                                      ? match_str.substr (1)
                                      : match_str;

                if (std::next (i) == vars_end)
                    final_desc += variable_match.suffix ();
            }

    dpp::presence new_presence ((dpp::presence_status)new_status,
                                (dpp::activity_type)new_type, final_desc);

    client->set_presence (new_presence);
    set_current_presence (client->cluster_id, new_presence);

    event.reply ("Done!");
}

} // musicat::command::owner::set_presence
