#include "musicat/audio_processing.h"
#include "musicat/child/command.h"
#include "musicat/child/dl_music.h"
#include "musicat/child/gnuplot.h"
#include "musicat/child/system.h"
#include "musicat/child/worker.h"
#include "musicat/child/ytdlp.h"
#include "musicat/musicat.h"
#include <linux/prctl.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace musicat::child::worker_command
{

int
create_audio_processor (command::command_options_t &options)
{
    // std::pair<int, int> pipe_fds = worker::create_pipe ();
    pid_t status = -1;

    // if (pipe_fds.first == -1)
    //     {
    //         return status;
    //     }
    // int read_fd = pipe_fds.first;
    // int write_fd = pipe_fds.second;

    const std::string as_fp
        = audio_processing::get_audio_stream_fifo_path (options.id);

    const std::string si_fp
        = audio_processing::get_audio_stream_stdin_path (options.id);

    const std::string so_fp
        = audio_processing::get_audio_stream_stdout_path (options.id);

    std::string sem_full_key;
    sem_t *sem;

    unlink (as_fp.c_str ());
    unlink (si_fp.c_str ());
    unlink (so_fp.c_str ());

    const auto fifo_bitmask
        = audio_processing::get_audio_stream_fifo_mode_t ();

    if ((status = mkfifo (as_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("cap as_fp");
            goto err1;
        }

    if ((status = mkfifo (si_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("cap si_fp");
            goto err2;
        }

    if ((status = mkfifo (so_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("cap so_fp");
            goto err3;
        }

    options.audio_stream_fifo_path = as_fp;
    options.audio_stream_stdin_path = si_fp;
    options.audio_stream_stdout_path = so_fp;

    sem_full_key = child::get_sem_key (options.id);
    sem = child::create_sem (sem_full_key);

    status = child::worker::call_fork ();

    if (status < 0)
        {
            perror ("cap fork");
            goto err4;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("create_audio_processor prctl");
                    child::do_sem_post (sem);
                    _exit (EXIT_FAILURE);
                }

            // close (read_fd);

            // options.child_write_fd = write_fd;

            child::do_sem_post (sem);

            status = audio_processing::run_processor (options);
            _exit (status);
        }

    // close (write_fd);

    child::do_sem_wait (sem, sem_full_key);

    options.pid = status;
    // options.parent_read_fd = read_fd;

    return 0;

err4:
    unlink (so_fp.c_str ());
    child::do_sem_post (sem);
    child::do_sem_wait (sem, sem_full_key);
err3:
    unlink (si_fp.c_str ());
err2:
    unlink (as_fp.c_str ());
err1:
    // close (read_fd);
    // close (write_fd);
    return status;
}

/**
 * Invoking ytdlp cmd and save the output to a json file
 *
 * Requires id, ytdlp_util_exe, ytdlp_lib_path and ytdlp_query
 * Optionally ytdlp_max_entries
 *
 * Creates a fifo based on id
 * Caller should open and read the fifo to get the cmd output
 * containing file_path of the result to open and read
 *
 * If file_path is empty then smt wrong happened
 * !TODO: result status?
 */
int
call_ytdlp (command::command_options_t &options)
{
    pid_t status = -1;

    const std::string as_fp = ytdlp::get_ytdout_fifo_path (options.id);

    std::string sem_full_key;
    sem_t *sem;

    unlink (as_fp.c_str ());

    const auto fifo_bitmask
        = audio_processing::get_audio_stream_fifo_mode_t ();

    if ((status = mkfifo (as_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("worker_command::call_ytdlp as_fp");
            goto err1;
        }

    sem_full_key = child::get_sem_key (options.id);
    sem = child::create_sem (sem_full_key);

    status = child::worker::call_fork ();

    if (status < 0)
        {
            perror ("worker_command::call_ytdlp fork");
            goto err4;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("call_ytdlp prctl");
                    child::do_sem_post (sem);
                    _exit (EXIT_FAILURE);
                }

            child::do_sem_post (sem);

            _exit (ytdlp::run (options, sem, sem_full_key));
        }

    child::do_sem_wait (sem, sem_full_key);

    options.pid = status;

    return 0;

err4:
    child::do_sem_post (sem);
    child::do_sem_wait (sem, sem_full_key);
    unlink (as_fp.c_str ());
err1:
    return status;
}

// unused anywhere yet, make sure fork waitpid and the likes been used
// correctly here!
int
run_gnuplot (const command::command_options_t &options)
{
    // read,write
    int crpw[2];
    int prcw[2];

    if (pipe (crpw) != 0)
        {
            perror ("pipe 1");
            return -1;
        }

    if (pipe (prcw) != 0)
        {
            perror ("pipe 2");
            return -1;
        }

    const std::string as_fp = gnuplot::get_gnuplot_out_fifo_path (options.id);
    int outfd = open (as_fp.c_str (), O_WRONLY);

    if (outfd == -1)
        {
            perror ("open outfd");
            return -2;
        }

    std::string sem_full_key = child::get_sem_key (options.id);
    sem_t *sem = child::create_sem (sem_full_key);

    // !TODO
    const char *GNUPLOT_CMD = options.gnuplot_cmd.c_str ();

    int child_write = prcw[1];
    int child_read = crpw[0];

    int parent_write = crpw[1];
    int parent_read = prcw[0];

    pid_t p = child::worker::call_fork ();

    if (p == 0)
        {
            // child
            /*
set term png size 1024,1024
*/
            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("call_gnuplot prctl");
                    child::do_sem_post (sem);
                    _exit (EXIT_FAILURE);
                }

            close (parent_write);
            close (parent_read);

            dup2 (child_read, STDIN_FILENO);
            dup2 (child_write, STDOUT_FILENO);

            close (child_write);
            close (child_read);

            char *args[] = { (char *)GNUPLOT_CMD, (char *)NULL };

            child::do_sem_post (sem);
            execvp (GNUPLOT_CMD, args);

            exit (EXIT_FAILURE);
        }

    child::do_sem_wait (sem, sem_full_key);

    close (child_write);
    close (child_read);

    int base_y_pos = 4;
    int y_pos_gap = 4;

    std::string label_x_pos = "66";

    // !TODO gnuplot_args
    std::string v65 = "100";
    std::string v92 = "100";
    std::string v131 = "100";
    std::string v185 = "100";
    std::string v262 = "100";
    std::string v370 = "100";
    std::string v523 = "100";
    std::string v740 = "100";
    std::string v1047 = "100";
    std::string v1480 = "100";
    std::string v2093 = "100";
    std::string v2960 = "100";
    std::string v4186 = "100";
    std::string v5920 = "100";
    std::string v8372 = "100";
    std::string v11840 = "100";
    std::string v16744 = "100";
    std::string v20000 = "100";

    std::string cmd1
        = "set term png size 1920,1080 font \"Mono,14\"\n"
          "set ylabel 'Volume Percentage'\n"
          "set xlabel 'Hz'\n"
          "set xrange [64:21000]\n"
          "set yrange [0:200]\n"
          "set logscale x\n"
          // "set logscale y\n"

          "set mxtics 18\n"
          "set ytics 10\n"
          // "set mytics 10\n"

          "set grid x y mx my\n"

          "$data << EOD\n"
          "65 "
          + v65
          + "\n"
            "92 "
          + v92
          + "\n"
            "131 "
          + v131
          + "\n"
            "185 "
          + v185
          + "\n"
            "262 "
          + v262
          + "\n"
            "370 "
          + v370
          + "\n"
            "523 "
          + v523
          + "\n"
            "740 "
          + v740
          + "\n"
            "1047 "
          + v1047
          + "\n"
            "1480 "
          + v1480
          + "\n"
            "2093 "
          + v2093
          + "\n"
            "2960 "
          + v2960
          + "\n"
            "4186 "
          + v4186
          + "\n"
            "5920 "
          + v5920
          + "\n"
            "8372 "
          + v8372
          + "\n"
            "11840 "
          + v11840
          + "\n"
            "16744 "
          + v16744
          + "\n"
            "20000 "
          + v20000
          + "\n"
            "EOD\n"
            "set label 1 'b1  65Hz    "
          + v65 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 17))
          + "\n"
            "set label 2 'b2  92Hz    "
          + v92 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 16))
          + "\n"
            "set label 3 'b3  131Hz   "
          + v131 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 15))
          + "\n"
            "set label 4 'b4  185Hz   "
          + v185 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 14))
          + "\n"
            "set label 5 'b5  262Hz   "
          + v262 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 13))
          + "\n"
            "set label 6 'b6  370Hz   "
          + v370 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 12))
          + "\n"
            "set label 7 'b7  523Hz   "
          + v523 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 11))
          + "\n"
            "set label 8 'b8  740Hz   "
          + v740 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 10))
          + "\n"
            "set label 9 'b9  1047Hz  "
          + v1047 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 9))
          + "\n"
            "set label 10 'b10 1480Hz  "
          + v1480 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 8))
          + "\n"
            "set label 11 'b11 2093Hz  "
          + v2093 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 7))
          + "\n"
            "set label 12 'b12 2960Hz  "
          + v2960 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 6))
          + "\n"
            "set label 13 'b13 4186Hz  "
          + v4186 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 5))
          + "\n"
            "set label 14 'b14 5920Hz  "
          + v5920 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 4))
          + "\n"
            "set label 15 'b15 8372Hz  "
          + v8372 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 3))
          + "\n"
            "set label 16 'b16 11840Hz "
          + v11840 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + (y_pos_gap * 2))
          + "\n"
            "set label 17 'b17 16744Hz "
          + v16744 + "' at " + label_x_pos + ","
          + std::to_string (base_y_pos + y_pos_gap)
          + "\n"
            "set label 18 'b18 20000Hz "
          + v20000 + "' at " + label_x_pos + "," + std::to_string (base_y_pos)
          + "\n"
            "unset k\n"
            "plot $data lt 7 ps 1.5 t \"Bands\", \"\" smooth sbezier lw 4\n";

    write (parent_write, cmd1.c_str (), cmd1.length () + 1);

    const size_t bufsiz = 4096;
    char buf[bufsiz];
    ssize_t cur_read = 0;

    struct pollfd prfds[1];
    prfds[0].events = POLLIN;
    prfds[0].fd = parent_read;

    int pollwait = 300;

    bool read_ready = true;

    while (read_ready && (cur_read = read (parent_read, buf, bufsiz)) > 0)
        {
            fprintf (stderr, "read %ld\n", cur_read);
            write (outfd, buf, cur_read);

            read_ready = (poll (prfds, 1, pollwait) > 0)
                         && (prfds[0].revents & POLLIN);
        }

    close (outfd);
    outfd = -1;

    char cmd2[] = "q\n";

    // child will exit with this cmd
    write (parent_write, cmd2, sizeof (cmd2) / sizeof (*cmd2));

    close (parent_write);
    close (parent_read);

    return 0;
}

int
call_gnuplot (command::command_options_t &options)
{
    pid_t status = -1;

    const std::string as_fp = gnuplot::get_gnuplot_out_fifo_path (options.id);

    std::string sem_full_key;
    sem_t *sem;

    unlink (as_fp.c_str ());

    const auto fifo_bitmask
        = audio_processing::get_audio_stream_fifo_mode_t ();

    if ((status = mkfifo (as_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("worker_command::call_gnuplot as_fp");
            goto err1;
        }

    sem_full_key = child::get_sem_key (options.id);
    sem = child::create_sem (sem_full_key);

    status = child::worker::call_fork ();

    if (status < 0)
        {
            perror ("worker_command::call_gnuplot fork");
            goto err4;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("call_gnuplot prctl");
                    child::do_sem_post (sem);
                    _exit (EXIT_FAILURE);
                }

            int gnuplot_status = run_gnuplot (options);

            child::do_sem_post (sem);

            _exit (gnuplot_status);
        }

    child::do_sem_wait (sem, sem_full_key);

    options.pid = status;

    return 0;

err4:
    child::do_sem_post (sem);
    child::do_sem_wait (sem, sem_full_key);
    unlink (as_fp.c_str ());
err1:
    return status;
}

int
call_system (command::command_options_t &options)
{
    pid_t status = -1;

    const std::string as_fp = system::get_system_fifo_path (options.id);

    std::string sem_full_key;
    sem_t *sem;

    unlink (as_fp.c_str ());

    const auto fifo_bitmask
        = audio_processing::get_audio_stream_fifo_mode_t ();

    if ((status = mkfifo (as_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("worker_command::call_system as_fp");
            goto err1;
        }

    sem_full_key = child::get_sem_key (options.id);
    sem = child::create_sem (sem_full_key);

    status = child::worker::call_fork ();

    if (status < 0)
        {
            perror ("worker_command::call_system fork");
            goto err4;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("call_system prctl");
                    child::do_sem_post (sem);
                    _exit (EXIT_FAILURE);
                }

            child::do_sem_post (sem);

            _exit (system::run (options, sem, sem_full_key));
        }

    child::do_sem_wait (sem, sem_full_key);

    options.pid = status;

    return 0;

err4:
    child::do_sem_post (sem);
    child::do_sem_wait (sem, sem_full_key);
    unlink (as_fp.c_str ());
err1:
    return status;
}

int
download_music (command::command_options_t &options)
{
    pid_t status = -1;

    const std::string as_fp
        = dl_music::get_download_music_fifo_path (options.id);

    std::string sem_full_key;
    sem_t *sem;

    unlink (as_fp.c_str ());

    const auto fifo_bitmask
        = audio_processing::get_audio_stream_fifo_mode_t ();

    if ((status = mkfifo (as_fp.c_str (), fifo_bitmask)) < 0)
        {
            perror ("worker_command::download_music as_fp");
            goto err1;
        }

    sem_full_key = child::get_sem_key (options.id);
    sem = child::create_sem (sem_full_key);

    status = child::worker::call_fork ();

    if (status < 0)
        {
            perror ("worker_command::download_music fork");
            goto err;
        }

    if (status == 0)
        {
            worker::handle_worker_fork ();

            if (prctl (PR_SET_PDEATHSIG, SIGTERM) == -1)
                {
                    perror ("download_music prctl");
                    child::do_sem_post (sem);
                    _exit (EXIT_FAILURE);
                }

            if (!options.debug)
                {
                    // redirect ffmpeg stderr to /dev/null
                    int dnull = open ("/dev/null", O_WRONLY);
                    dup2 (dnull, STDERR_FILENO);
                    close_valid_fd (&dnull);
                }

            child::do_sem_post (sem);

            int notif_fifo = open (as_fp.c_str (), O_WRONLY);
            if (notif_fifo < 0)
                {
                    perror ("worker_command::download_music::child notif_fifo "
                            "open");

                    _exit (EXIT_FAILURE);
                }

            // dup stdout to notif fifo
            if (dup2 (notif_fifo, STDOUT_FILENO) == -1)
                {
                    perror ("worker_command::download_music::child notif_fifo "
                            "dout");

                    close_valid_fd (&notif_fifo);

                    _exit (EXIT_FAILURE);
                }
            close_valid_fd (&notif_fifo);

            /*
                yt_dlp -f 251 --http-chunk-size 2M $URL -x
                --audio-format opus --audio-quality 0 -o $FILEPATH
            */

            const char *yt_dlp = options.ytdlp_util_exe.c_str ();
            const char *url = options.ytdlp_query.c_str ();
            const char *fpath = options.file_path.c_str ();
            char *args[] = { (char *)yt_dlp,
                             "-f",
                             "251",
                             "--http-chunk-size",
                             "2M",
                             (char *)url,
                             "-x",
                             "--audio-format",
                             "opus",
                             "--audio-quality",
                             "0",
                             "-o",
                             (char *)fpath,
                             (char *)NULL };

            int cmd_status = execvp (yt_dlp, args);

            perror ("worker_command::download_music::child exit");
            _exit (cmd_status);
        }

    child::do_sem_wait (sem, sem_full_key);
    options.pid = status;

    return 0;

err:
    child::do_sem_post (sem);
    child::do_sem_wait (sem, sem_full_key);
err1:
    unlink (as_fp.c_str ());

    return status;
}

} // musicat::child::worker_command
