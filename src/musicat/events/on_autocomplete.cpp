#include "musicat/events/on_autocomplete.h"
#include "musicat/cmds/download.h"
#include "musicat/cmds/image.h"
#include "musicat/cmds/play.h"
#include "musicat/cmds/playlist.h"

namespace musicat::events
{
void
on_autocomplete (dpp::cluster *client)
{
    client->on_autocomplete ([] (const dpp::autocomplete_t &event) {
        const std::string cmd = event.name;
        std::string opt = "";
        std::vector<std::string> sub_cmd = {};
        std::string param = "";

        bool sub_level = true;

        for (const auto &i : event.options)
            {
                if (!i.focused)
                    continue;

                opt = i.name;
                if (std::holds_alternative<std::string> (i.value))
                    param = std::get<std::string> (i.value);

                sub_level = false;
                break;
            }

        // int cur_sub = 0;
        std::vector<dpp::command_data_option> eopts
            = event.command.get_autocomplete_interaction ().options;

        while (sub_level && eopts.begin () != eopts.end ())
            {
                // cur_sub++;
                auto sub = eopts.at (0);
                sub_cmd.push_back (sub.name);
                for (const auto &i : sub.options)
                    {
                        if (!i.focused)
                            continue;

                        opt = i.name;
                        if (std::holds_alternative<std::string> (i.value))
                            param = std::get<std::string> (i.value);

                        sub_level = false;
                        break;
                    }

                if (!sub_level)
                    break;

                eopts = sub.options;
            }

        if (!opt.empty ())
            {
                if (cmd == "play")
                    {
                        if (opt == "query")
                            command::play::autocomplete::query (event, param);
                    }
                else if (cmd == "playlist")
                    {
                        if (opt == "id")
                            command::playlist::autocomplete::id (event, param);
                    }
                else if (cmd == "download")
                    {
                        if (opt == "track")
                            command::download::autocomplete::track (event,
                                                                    param);
                    }
                else if (cmd == "image")
                    {
                        if (opt == "type")
                            command::image::autocomplete::type (event, param);
                    }
            }
    });
}
} // musicat::events
