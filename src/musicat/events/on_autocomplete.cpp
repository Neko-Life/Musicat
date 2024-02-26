#include "musicat/events/on_autocomplete.h"
#include "musicat/cmds/download.h"
#include "musicat/cmds/filters.h"
#include "musicat/cmds/image.h"
#include "musicat/cmds/play.h"
#include "musicat/cmds/playlist.h"
#include "musicat/util.h"

namespace musicat::events
{
struct autocomplete_handler_t
{
    void (*handler) (const dpp::autocomplete_t &, const std::string &);
    std::vector<const char *> paths;
};

inline const autocomplete_handler_t handlers[]
    = { { command::play::autocomplete::query, { "play", "query" } },
        { command::playlist::autocomplete::id, { "playlist", "load", "id" } },
        { command::playlist::autocomplete::id, { "playlist", "view", "id" } },
        { command::playlist::autocomplete::id, { "playlist", "save", "id" } },
        { command::playlist::autocomplete::id,
          { "playlist", "delete", "id" } },
        { command::download::autocomplete::track, { "download", "track" } },
        { command::image::autocomplete::type, { "image", "type" } },
        { command::filters::equalizer_presets::autocomplete::name,
          { "filters", "equalizer_presets", "load", "name" } },
        { command::filters::equalizer_presets::autocomplete::name,
          { "filters", "equalizer_presets", "view", "name" } },
        { NULL, {} } };

void
on_autocomplete (dpp::cluster *client)
{
    client->on_autocomplete ([] (const dpp::autocomplete_t &event) {
        auto focused = util::find_focused (
            event.command.get_autocomplete_interaction ().options,
            { event.name });

        const std::string &opt = focused.focused.name;

        if (opt.empty ())
            return;

        size_t fpsiz = focused.paths.size ();

        for (size_t i = 0; handlers[i].handler; i++)
            {
                const auto &handler_data = handlers[i];

                if (handler_data.paths.size () != fpsiz)
                    continue;

                size_t max_idx = fpsiz - 1;
                for (size_t i = 0; i < fpsiz; i++)
                    {
                        if (focused.paths.at (i) != handler_data.paths.at (i))
                            {
                                break;
                            }

                        if (i == max_idx)
                            {
                                std::string param;

                                if (std::holds_alternative<std::string> (
                                        focused.focused.value))
                                    param = std::get<std::string> (
                                        focused.focused.value);

                                handler_data.handler (event, param);
                            }
                    }
            }
    });
}
} // musicat::events
