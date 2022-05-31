#ifndef MUSICAT_COMMAND_H
#define MUSICAT_COMMAND_H

#include <dpp/dpp.h>
#include "musicat/musicat.h"
#include "musicat/player.h"

namespace musicat_command {
    namespace mc = musicat;
    namespace mpl = musicat_player;
    using string = std::string;
    using player_manager_ptr = std::shared_ptr<mpl::Manager>;

    namespace hello {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event);
    }

    namespace invite {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event);
    }

    namespace pause {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);
    }

    namespace skip {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);
    }

    namespace play {
        namespace autocomplete {
            void query(const dpp::autocomplete_t& event, player_manager_ptr player_manager, dpp::cluster& client);
        }

        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager, const dpp::snowflake sha_id);
    }

    namespace loop {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager, const dpp::snowflake sha_id);
    }

    namespace queue {
        enum queue_modify_t : int8_t {
            // Shuffle the queue
            m_shuffle,
            // Reverse the queue
            m_reverse,
            // Clear songs added by users who left the vc
            m_clear_left,
            // Clear queue
            m_clear
        };

        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);
    }
}

#endif // MUSICAT_COMMAND_H
