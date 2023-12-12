#include "musicat/eliza.h"
#include "musicat/musicat.h"
#include <filesystem>
#include <regex>
#include <stdio.h>
#include <sys/stat.h>

namespace musicat::eliza
{

void
handle_message_create (const dpp::message_create_t &event)
{
    if (!get_running_state ())
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

    std::string msg_content = event.msg.content;

    std::regex re_m (" ?<@!?" + sid.str () + ">");
    std::regex re_s ("^\\s+|\\s+$");

    msg_content = std::regex_replace (msg_content, re_m, "");
    msg_content = std::regex_replace (msg_content, re_s, "");

    if (msg_content.empty ())
        return;

    std::string resp = eliza::ask (msg_content);

    if (resp.empty ())
        return;

    for (char &c : resp)
        {
            c = std::tolower (c);
        }

    event.reply (resp);
}

} // musicat::eliza
