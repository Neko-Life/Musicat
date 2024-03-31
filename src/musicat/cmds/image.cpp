#include "musicat/cmds/image.h"
#include "musicat/autocomplete.h"
#include "musicat/musicat.h"
#include "musicat/util.h"
#include "nekos-best++.hpp"

namespace musicat::command::image
{
namespace autocomplete
{

void
type (const dpp::autocomplete_t &event, const std::string &param)
{

    std::vector<std::pair<std::string, std::string> > avail = {};

    const bool no_len = param.empty ();

    const auto endpoints = get_cached_nekos_best_endpoints ();
    avail.reserve (endpoints.size ());

    for (const auto &i : endpoints)
        avail.push_back (std::make_pair (i.second.name, i.second.name));

    if (!no_len)
        avail = musicat::autocomplete::filter_candidates (avail, param);

    if (get_debug_state ())
        {
            util::print_autocomplete_results (avail,
                                              "image::autocomplete::type");
        }

    musicat::autocomplete::create_response (avail, event);
}

} // autocomplete

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("image", "Send [random] image", sha_id)
        .add_option (dpp::command_option (dpp::co_string, "type",
                                          "Type [of image] to send", true)
                         .set_auto_complete (true));
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::string img_type = "";
    get_inter_param (event, "type", &img_type);

    if (img_type.empty ())
        {
            event.reply ("Provide `type`");
            return;
        }

    event.thinking ();

    nekos_best::QueryResult data = nekos_best::fetch (img_type, 1);

    if (!data.results.size ())
        {
            event.edit_response ("`[ERROR]` No result");
            return;
        }

    const nekos_best::Meta &metadata = data.results.at (0);

    if (metadata.url.empty ())
        {
            event.edit_response ("`[ERROR]` Empty result");
            return;
        }

    dpp::embed emb;

    emb.set_image (metadata.url);
    emb.set_color (dpp::colors::scarlet_red);

    dpp::message mes;

    mes.add_embed (emb);

    event.edit_response (mes);
}

} // musicat::command::image
