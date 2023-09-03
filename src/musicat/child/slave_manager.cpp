#include "musicat/child/worker.h"
#include "musicat/child/worker_management.h"
#include <map>
#include <string>
#include <sys/wait.h>

namespace musicat
{
namespace child
{
namespace slave_manager
{

std::map<std::string, command::command_options_t> slave_list;

int
insert_slave (command::command_options_t &options)
{
    auto i = slave_list.find (options.id);
    if (i != slave_list.end ())
        {
            return 1;
        }

    slave_list.insert (std::make_pair (options.id, options));
    return 0;
}

std::pair<int, command::command_options_t>
get_slave (std::string &id)
{
    auto i = slave_list.find (id);
    if (i == slave_list.end ())
        {
            return { -1, command::create_command_options () };
        }

    return { 0, i->second };
}

int
delete_slave (std::string &id)
{
    auto i = slave_list.find (id);
    if (i == slave_list.end ())
        {
            return 1;
        }

    slave_list.erase (i);
    return 0;
}

int
shutdown_routine (command::command_options_t &options)
{
    std::string child_type = options.command;

    if (child_type
        == command::command_execute_commands_t.create_audio_processor)
        {
            return worker_management::shutdown_audio_processor (options);
        }

    return 1;
}

int
wait_routine (command::command_options_t &options)
{
    int status = 0;
    waitpid (options.pid, &status, 0);
    fprintf (stderr,
             "[child::slave_manager] Child %s with pid %d exited with "
             "status %d\n",
             options.id.c_str (), options.pid, status);

    return status;
}

int
clean_up_routine (command::command_options_t &options)
{
    // usually we close parent read fd here but it's already
    // closed on shutdown for all processor child
    // new type of child might need to be handled differently
    // !TODO: clean up to be made fifos
    std::string child_type = options.command;

    if (child_type
        == command::command_execute_commands_t.create_audio_processor)
        {
            return worker_management::clean_up_audio_processor (options);
        }

    return 0;
}

int
shutdown (std::string &id)
{
    auto i = slave_list.find (id);
    if (i == slave_list.end ())
        {
            return 1;
        }

    shutdown_routine (i->second);

    return 0;
}

int
wait (std::string &id)
{
    auto i = slave_list.find (id);
    if (i == slave_list.end ())
        {
            return 1;
        }

    wait_routine (i->second);

    return 0;
}

int
clean_up (std::string &id)
{
    auto i = slave_list.find (id);
    if (i == slave_list.end ())
        {
            return 1;
        }

    clean_up_routine (i->second);

    slave_list.erase (i);

    return 0;
}

int
shutdown_all ()
{
    auto i = slave_list.begin ();
    while (i != slave_list.end ())
        {
            shutdown_routine (i->second);

            i++;
        }

    return 0;
}

int
wait_all ()
{
    auto i = slave_list.begin ();
    while (i != slave_list.end ())
        {
            wait_routine (i->second);

            i++;
        }

    return 0;
}

int
clean_up_all ()
{
    auto i = slave_list.begin ();
    while (i != slave_list.end ())
        {
            clean_up_routine (i->second);

            i = slave_list.erase (i);
        }

    return 0;
}

} // slave_manager
} // child
} // musicat
