#include "musicat/runtime_cli.h"
#include "musicat/musicat.h"
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

void
_print_pad (size_t len)
{
    if ((long)len < 0L)
        return;

    for (size_t i = 0; i < len; i++)
        {
            printf (" ");
        }
}

namespace musicat
{
namespace runtime_cli
{
bool attached = false;

int
attach_listener ()
{
    if (attached == true)
        {
            fprintf (stderr,
                     "[ERROR RUNTIME_CLI] stdin listener already attached!\n");
            return 1;
        }
    printf ("[INFO] Enter `-d` to toggle debug mode\n");

    std::thread stdin_listener ([] () {
        attached = true;

        const std::map<std::pair<const char *, const char *>, const char *>
            commands = {
                { { "command", "alias" }, "description" },
                { { "help", "-h" }, "Print this message" },
                { { "debug", "-d" }, "Toggle debug mode" },
                { { "clear", "-c" }, "Clear console" },
            };

        size_t padding_command = 0;
        size_t padding_alias = 0;

        for (std::pair<std::pair<const char *, const char *>, const char *>
                 desc : commands)
            {
                const size_t len_command = strlen (desc.first.first) + 1U;
                const size_t len_alias = strlen (desc.first.second) + 2U;
                if (len_command > padding_command)
                    padding_command = len_command;
                if (len_alias > padding_alias)
                    padding_alias = len_alias;
            }

        while (get_running_state ())
            {
                char cmd[128];
                memset (cmd, '\0', sizeof (cmd));
                scanf ("%s", cmd);

                if (strcmp (cmd, "help") == 0 || strcmp (cmd, "-h") == 0)
                    {
                        printf ("Usage: [command] [args] <ENTER>\n\n");

                        for (std::pair<std::pair<const char *, const char *>,
                                       const char *>
                                 desc : commands)
                            {
                                printf ("%s", desc.first.first);
                                _print_pad (padding_command
                                            - strlen (desc.first.first));
                                printf (":");
                                const size_t pad_a
                                    = padding_alias
                                      - strlen (desc.first.second);
                                _print_pad (pad_a / 2);
                                printf ("%s", desc.first.second);
                                _print_pad ((size_t)ceil ((double)pad_a
                                                          / (double)2.0));
                                printf (": %s\n", desc.second);
                            }
                    }
                else if (strcmp (cmd, "debug") == 0 || strcmp (cmd, "-d") == 0)
                    {
                        set_debug_state (!get_debug_state ());
                    }
                else if (strcmp (cmd, "clear") == 0 || strcmp (cmd, "-c") == 0)
                    {
                        system ("clear");
                    }
            }
    });
    stdin_listener.detach ();

    return 0;
}
}
}
