#include "musicat/eliza.h"
#include "musicat/musicat.h"
#include <filesystem>
#include <stdio.h>
#include <sys/stat.h>

namespace musicat::eliza
{

int
check ()
{
    struct stat ofile_stat;

    bool serr = false;
    if ((serr = (stat ("eliza/script.txt", &ofile_stat) != 0))
        || !S_ISREG (ofile_stat.st_mode))
        {
            if (serr)
                perror ("eliza");

            std::filesystem::create_directory ("eliza");

            fprintf (stderr, "[ERROR] Missing eliza script, please provide it "
                             "under eliza/ directory\nIn the meantime, Eliza "
                             "chatbot will be disabled\n");

            return 1;
        }

    return 0;
}

void
handle_message_create (const dpp::message_create_t &event)
{
    ofxEliza *eliza_ptr = get_eliza ();

    if (!eliza_ptr)
        return;

    auto sid = get_sha_id ();
    if (event.msg.author.id == sid)
        return;

    // check if actually mentioned
    bool mentioned = false;

    for (const auto &[user, member] : event.msg.mentions)
        {
            if (user.id != sid)
                continue;

            mentioned = true;
            break;
        }

    if (!mentioned)
        return;

    if (event.msg.content.empty ())
        return;

    std::string resp = eliza_ptr->ask (event.msg.content);

    if (resp.empty ())
        return;

    event.reply (resp);
}

} // musicat::eliza
