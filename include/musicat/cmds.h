#ifndef MUSICAT_COMMAND_H
#define MUSICAT_COMMAND_H

#include <dpp/dpp.h>
#include <vector>
#include <mutex>
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
            void query(const dpp::autocomplete_t& event, string param, player_manager_ptr player_manager, dpp::cluster& client);
        }

        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);

        /**
         * @brief Search and add track to guild queue, can be used for interaction and non interaction. Interaction must have already deferred/replied
         *
         * @param playlist Whether arg_query is youtube playlist url or search query
         * @param guild_id Guild which data to be updated with
         * @param arg_query Valid youtube url or search query
         * @param arg_top Whether to add the track to the top of the queue or not
         * @param vcclient_cont Whether client is in a voice channel or not
         * @param v Voice connection, can be NULL
         * @param channel_id Target voice channel for the client to join and play tracks to
         * @param sha_id Client user Id
         * @param player_manager Player manager ptr
         * @param from_interaction Whether from an interaction or not
         * @param from Discord client used to reconnect/join voice channel
         * @param event Can be incomplete type or filled if from interaction
         * @param continued Whether marker to initialize playback has been inserted
         */
        void add_track(bool playlist,
            dpp::snowflake guild_id,
            string arg_query,
            int64_t arg_top,
            bool vcclient_cont,
            dpp::voiceconn* v,
            const dpp::snowflake channel_id,
            const dpp::snowflake sha_id,
            player_manager_ptr player_manager,
            bool from_interaction,
            dpp::discord_client* from,
            const dpp::interaction_create_t event = dpp::interaction_create_t(NULL, "{}"),
            bool continued = false);
    }

    namespace loop {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);
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

    namespace autoplay {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);
    }

    namespace move {
        namespace autocomplete {
            void track(const dpp::autocomplete_t& event, string param, player_manager_ptr player_manager, dpp::cluster& client);
        }

        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);
    }

    namespace remove {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event, player_manager_ptr player_manager);
    }

    namespace bubble_wrap {
        dpp::slashcommand get_register_obj(const dpp::snowflake sha_id);
        void slash_run(const dpp::interaction_create_t& event);
    }

    namespace search {
        dpp::slashcommand get_register_obj(const dpp::snowflake& sha_id);
        dpp::interaction_modal_response modal_enqueue_searched_track();
        void slash_run(const dpp::interaction_create_t& event);
    }
}

#endif // MUSICAT_COMMAND_H
