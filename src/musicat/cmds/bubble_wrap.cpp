#include "musicat/cmds/bubble_wrap.h"
#include "musicat/cmds.h"

namespace musicat::command::bubble_wrap
{
dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    return dpp::slashcommand ("bubble_wrap", "Pop dems bubbles!!!", sha_id);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    static const std::string bubble_wrap
        = "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||\n   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||\n||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||\n   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||\n||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||\n   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||\n||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||\n   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||\n||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||\n   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||\n||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||\n   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||\n||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||\n   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   ||O||   "
          "||O||   ||O||   ||O||";
    event.reply (std::string ("YAY bubblewrap for stressed developers 😄\n")
                 + bubble_wrap);
}
} // musicat::command::bubble_wrap
