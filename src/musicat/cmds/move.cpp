#include "musicat/cmds.h"

namespace musicat
{
namespace command
{
namespace move
{
namespace autocomplete
{
void
track (const dpp::autocomplete_t &event, std::string param,
       player::player_manager_ptr player_manager, dpp::cluster &client)
{
    std::map<size_t, std::string> avail = {};
    auto player = player_manager->get_player (event.command.guild_id);

    size_t siz = 0;
    dpp::interaction_response r (dpp::ir_autocomplete_reply);
    if (player)
        {
            std::lock_guard<std::mutex> lk (player->q_m);
            siz = player->queue.size ();
        }
    if (siz < 2)
        {
            client.interaction_response_create (event.command.id,
                                                event.command.token, r);
            return;
        }

    if (!param.length ())
        {
            std::lock_guard<std::mutex> lk (player->q_m);
            for (size_t i = 1; i < siz; i++)
                {
                    avail[i] = player->queue[i].title ();
                    if (i == 25)
                        break;
                }
        }
    else
        {
            std::lock_guard<std::mutex> lk (player->q_m);
            for (size_t a = 1; a < siz; a++)
                {
                    const auto ori = player->queue[a].title ();
                    auto i = ori;
                    for (auto &j : i)
                        j = std::tolower (j);
                    for (auto &j : param)
                        j = std::tolower (j);
                    if (i.find (param) != std::string::npos)
                        avail[a] = ori;
                    if (a == 25)
                        break;
                }
        }
    for (const auto &i : avail)
        {
            std::string nm = std::to_string (i.first) + ": " + i.second;
            r.add_autocomplete_choice (dpp::command_option_choice (
                nm.length () > 100 ? nm.substr (0, 100) : nm,
                (int64_t)i.first));
        }
    client.interaction_response_create (event.command.id, event.command.token,
                                        r);
}
}

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("move", "Move [track position] around the queue",
                              sha_id)
        .add_option (dpp::command_option (dpp::co_integer, "track",
                                          "Move [this] track", true))
        .add_option (dpp::command_option (dpp::co_integer, "to",
                                          "Move [to this] position", true));
}

void
slash_run (const dpp::interaction_create_t &event,
           player::player_manager_ptr player_manager)
{
    auto p = player_manager->get_player (event.command.guild_id);
    size_t queue_siz = 0;
    if (p)
        {
            std::lock_guard<std::mutex> lk (p->q_m);
            queue_siz = p->queue.size ();
        }
    int64_t max_to = (int64_t)queue_siz - 1;
    if (2 > max_to)
        {
            event.reply ("No moveable track in the queue");
            return;
        }

    int64_t fr = 0;
    int64_t to = 0;
    get_inter_param (event, "track", &fr);
    get_inter_param (event, "to", &to);

    if (fr < 1)
        fr = 1;
    if (to < 1)
        to = 1;
    if (fr > max_to)
        fr = max_to;
    if (to > max_to)
        to = max_to;

    player::MCTrack t;
    {
        std::lock_guard<std::mutex> lk (p->q_m);
        t = p->queue.at (fr);
    }

    if (fr != to)
        {
            std::string a;
            std::string b;
            {
                std::lock_guard<std::mutex> lk (p->q_m);
                a = p->queue.at (1).title ();
                b = p->queue.back ().title ();

                p->queue.erase (p->queue.begin () + fr);
                p->queue.insert (p->queue.begin () + to, t);
            }

            if (a != p->queue.at (1).title ()
                || b != p->queue.back ().title ())
                try
                    {
                        player_manager->update_info_embed (
                            event.command.guild_id);
                    }
                catch (...)
                    {
                    }
        }
    event.reply (std::string ("Moved ") + t.title () + " to position "
                 + std::to_string (to));
}
}
}
}
