#ifndef MUSICAT_CHILD_SLAVE_MANAGER_H
#define MUSICAT_CHILD_SLAVE_MANAGER_H

#include "musicat/child/command.h"

namespace musicat
{
namespace child
{
namespace slave_manager
{

int insert_slave (command::command_options_t &options);

std::pair<int, command::command_options_t> get_slave (std::string &id);

int delete_slave (std::string &id);

int shutdown (std::string &id);

int wait (std::string &id, bool force_kill);

int clean_up (std::string &id);

int shutdown_all ();

int wait_all ();

int clean_up_all ();

} // slave_manager
} // child
} // musicat

#endif // MUSICAT_CHILD_SLAVE_MANAGER_H
