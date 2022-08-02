#include "musicat/cmds.h"
#include "musicat/pagination.h"

namespace musicat {
    namespace command {
        namespace queue {
            dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
                return dpp::slashcommand(
                    "queue",
                    "Show or modify [tracks in the] queue",
                    sha_id
                ).add_option(
                    dpp::command_option(
                        dpp::co_integer,
                        "action",
                        "Modify [tracks in the] queue"
                    ).add_choice(
                        dpp::command_option_choice("Shuffle", queue_modify_t::m_shuffle)
                    ).add_choice(
                        dpp::command_option_choice("Reverse", queue_modify_t::m_reverse)
                    ).add_choice(
                        dpp::command_option_choice("Clear Left", queue_modify_t::m_clear_left)
                    ).add_choice(
                        dpp::command_option_choice("Clear", queue_modify_t::m_clear)
                    )
                );
            }

            void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager)
            {
                auto queue = player_manager->get_queue(event.command.guild_id);
                if (queue.empty())
                {
                    event.reply("No track");
                    return;
                }

                int64_t qarg = -1;
                musicat::get_inter_param(event, "action", &qarg);

                if (qarg > -1)
                {
                    switch (qarg)
                    {
                    case queue_modify_t::m_clear: {
                        auto siz = queue.size();
                        if (siz < 2)
                        {
                            event.reply("No track to clear, skip the current song to clear the queue completely");
                            break;
                        }
                        auto p = player_manager->get_player(event.command.guild_id);
                        p->reset_shifted();
                        std::lock_guard<std::mutex> lk(p->q_m);
                        musicat::player::MCTrack t = p->queue.at(0);
                        p->queue.clear();
                        p->queue.push_back(t);
                        player_manager->update_info_embed(event.command.guild_id);
                        event.reply("Cleared");

                        break;
                    }
                    case queue_modify_t::m_shuffle: {
                        if (player_manager->shuffle_queue(event.command.guild_id))
                        {
                            event.reply("Shuffled" + std::string(queue.size() < 5 ? ", I suggest the `/move` command if you do think my shuffling skill isn't very much satisfying" : ""));
                            break;
                        }
                        else
                        {
                            event.reply("Doesn't seem like shuffleable");
                            break;
                        }
                    }
                    case queue_modify_t::m_reverse: {
                        auto siz = queue.size();
                        if (siz < 3)
                        {
                            event.reply("Reversed. Not that you can tell the difference");
                            break;
                        }
                        auto p = player_manager->get_player(event.command.guild_id);
                        p->reset_shifted();
                        {
                            std::lock_guard<std::mutex> lk(p->q_m);
                            musicat::player::MCTrack t = p->queue.at(0);

                            std::deque<musicat::player::MCTrack> n_queue = {};
                            for (size_t i = (siz - 1); i > 0; i--)
                                n_queue.push_back(p->queue.at(i));

                            p->queue.clear();
                            p->queue = n_queue;
                            p->queue.push_front(t);
                        }
                        player_manager->update_info_embed(event.command.guild_id);
                        event.reply("Reversed");

                        break;
                    }
                    case queue_modify_t::m_clear_left: {
                        if (!player_manager->voice_ready(event.command.guild_id, event.from, event.command.usr.id))
                        {
                            event.reply("Establishing connection. Please wait...");
                            break;
                        }
                        size_t rmed = 0;
                        std::vector<dpp::snowflake> l_user = {};

                        auto p = player_manager->get_player(event.command.guild_id);
                        try
                        {
                            auto vc = musicat::get_voice_from_gid(event.command.guild_id, player_manager->sha_id);
                            {
                                std::lock_guard<std::mutex> lk(p->q_m);
                                for (auto i = p->queue.begin(); i != p->queue.end(); i++)
                                {
                                    bool cont = false;
                                    for (const auto& j : vc.second)
                                    {
                                        if (j.second.user_id == i->user_id)
                                        {
                                            cont = true;
                                            break;
                                        }
                                    }
                                    if (cont) continue;
                                    if (musicat::vector_find(&l_user, i->user_id) == l_user.end())
                                        l_user.push_back(i->user_id);
                                }
                            }
                            for (const auto& i : l_user)
                                rmed += p->remove_track_by_user(i);
                        }
                        catch (...)
                        {
                            event.reply("`ERROR`: Can't get current voice connection");
                            break;
                        }
                        size_t usiz = l_user.size();
                        if (!usiz)
                        {
                            event.reply("No user left this session yet");
                            break;
                        }
                        player_manager->update_info_embed(event.command.guild_id);
                        event.reply(std::to_string(usiz)
                            + " user" + std::string(usiz > 1 ? "s" : "")
                            + " left this session. Removed "
                            + std::to_string(rmed) + " track" + std::string(rmed > 1 ? "s" : "")
                        );

                        break;
                    }
                    default: event.reply("Option not yet supported and is still in development");
                    }
                    return;
                }

                std::vector<dpp::embed> embeds = {};

                dpp::embed embed;
                // std::vector<std::string> queue_str;
                std::string desc = "";
                size_t id = 1;
                size_t qs = queue.size();
                size_t count = 0;

                uint64_t totald = 0;

                for (auto i = queue.begin(); i != queue.end(); i++)
                    if (!i->info.raw.is_null())
                        totald += i->info.duration();
                for (auto i = queue.begin(); i != queue.end(); i++)
                {
                    uint64_t dur = 0;
                    if (!i->info.raw.is_null()) dur = i->info.duration();
                    if (i == queue.begin())
                    {
                        desc += "Current track: [" + i->title() + "](" + i->url() + ")"
                            + std::string(dur ? std::string(" [") + musicat::format_duration(dur) + "]" : "")
                            + " - <@" + std::to_string(i->user_id) + ">\n\n";
                    }
                    else
                    {
                        desc += std::to_string(id) + ": [" + i->title() + "](" + i->url() + ")"
                            + std::string(dur ? std::string(" [") + musicat::format_duration(dur) + "]" : "")
                            + " - <@" + std::to_string(i->user_id) + ">\n";
                        id++;
                        count++;
                    }
                    if ((count && !(count % 10)) || id == qs)
                    {
                        embed.set_title("Queue")
                            .set_description(desc.length() > 2048 ? "Description too long, pagination is on the way!" : desc);
                        if (totald) embed.set_footer(musicat::format_duration(totald), "");
                        embeds.emplace_back(embed);
                        embed = dpp::embed();
                        desc = "";
                        count = 0;
                    }
                }

                dpp::message msg;
                msg.add_embed(embeds.front());
                bool paginate = embeds.size() > 1;
                if (paginate)
                {
                    musicat::paginate::add_pagination_buttons(&msg);
                }

                event.reply(msg, musicat::paginate::get_inter_reply_cb(event, paginate, player_manager->cluster, embeds));
            }
        }
    }
}
