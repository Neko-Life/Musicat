#include "musicat/cmds.h"

namespace musicat_command {
    namespace hello {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id) {
            return dpp::slashcommand("hello", "Hello World!", sha_id);
        }

        void slash_run(const dpp::interaction_create_t& event) {
            event.reply("Hello there!!");
        }
    }
}
