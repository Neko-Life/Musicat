#ifndef MUSICAT_COMMAND_QUEUE_H
#define MUSICAT_COMMAND_QUEUE_H

#include <dpp/dpp.h>

namespace musicat::command::queue
{
enum queue_modify_t : int8_t
{
    // Shuffle the queue
    m_shuffle,
    // Reverse the queue
    m_reverse,
    // Clear songs added by users who left the vc
    m_clear_left,
    // Clear queue
    m_clear,
    // clear songs added by musicat
    m_clear_musicat
};

dpp::slashcommand get_register_obj (const dpp::snowflake &sha_id);
void slash_run (const dpp::slashcommand_t &event);
} // musicat::command::queue

#endif // MUSICAT_COMMAND_QUEUE_H
