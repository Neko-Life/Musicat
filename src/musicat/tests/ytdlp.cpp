#include "musicat/child/ytdlp.h"
#include "musicat/child/command.h"
#include "musicat/child/worker.h"
#include "musicat/musicat.h"
#include "musicat/tests.h"
#include "musicat/util/base64.h"

namespace musicat::tests
{

void
test_ytdlp ()
{
    // test ytdlp, id, ytdlp_util_exe, ytdlp_lib_path and ytdlp_query
    namespace cc = child::command;

    std::string q = "ytsearch25:How to train your dragon";
    std::string qid = util::base64::encode (q);

    std::string ytdlp_cmd
        = cc::command_options_keys_t.command + '='
          + cc::command_execute_commands_t.call_ytdlp + ';'
          + cc::command_options_keys_t.id + '=' + qid + ';'
          + cc::command_options_keys_t.ytdlp_util_exe + '='
          + cc::sanitize_command_value (get_ytdlp_util_exe ()) + ';'
          + cc::command_options_keys_t.ytdlp_lib_path + '='
          + cc::sanitize_command_value (get_ytdlp_lib_path ()) + ';'
          + cc::command_options_keys_t.ytdlp_query + '='
          + cc::sanitize_command_value (q) + ';';

    const std::string exit_cmd
        = cc::command_options_keys_t.id + '=' + qid + ';'
          + cc::command_options_keys_t.command + '='
          + cc::command_execute_commands_t.shutdown + ';';

    cc::send_command (ytdlp_cmd);

    int status = cc::wait_slave_ready (qid, 10);

    if (status == child::worker::ready_status_t.ERR_SLAVE_EXIST)
        {
            // status won't be 0 if this block is executed
            const std::string force_exit_cmd
                = exit_cmd + cc::command_options_keys_t.force + "=1;";

            cc::send_command (force_exit_cmd);
        }

    if (status != 0)
        {
            throw 2;
        }

    std::string out_fifo_path = child::ytdlp::get_ytdout_fifo_path (qid);

    int out_fifo = open (out_fifo_path.c_str (), O_RDONLY);

    char cmd_buf[CMD_BUFSIZE + 1];
    ssize_t sizread = read (out_fifo, cmd_buf, CMD_BUFSIZE);
    cmd_buf[sizread] = '\0';

    fprintf (stderr, "Got output:\n%s\n", cmd_buf);

    cc::command_options_t opt = cc::create_command_options ();
    cc::parse_command_to_options (cmd_buf, opt);

    fprintf (stderr, "id: %s\n", opt.id.c_str ());
    fprintf (stderr, "command: %s\n", opt.command.c_str ());
    fprintf (stderr, "file_path: %s\n", opt.file_path.c_str ());

    cc::send_command (exit_cmd);

    nlohmann::json json_res;

    std::ifstream scs (opt.file_path);
    if (!scs.is_open ())
        {
            return;
        }

    scs >> json_res;
    scs.close ();

    fprintf (stderr, "%s\n", json_res.dump (2).c_str ());
}
} // musicat::tests
