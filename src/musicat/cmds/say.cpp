#include "musicat/cmds/say.h"
#include "musicat/musicat.h"
#include "musicat/thread_manager.h"
#include "restresults.h"

namespace musicat::command::say
{
struct msg_source_t
{
    dpp::snowflake channel_id;
    dpp::snowflake msg_id;
};

msg_source_t
parse_msg_source (const std::string &content)
{
    msg_source_t ret;

    constexpr const char prefix[] = "<:msg:";
    constexpr const size_t psiz = sizeof (prefix) / sizeof (*prefix);

    size_t end = 0;
    if (content.find (prefix) != 0
        || (end = content.find (":>", psiz)) == std::string::npos)
        return ret;

    const std::string sc_ids = content.substr (psiz, end - psiz);
    const size_t channel_id_end = sc_ids.find (':');

    if (channel_id_end != std::string::npos)
        {
            const std::string cid = sc_ids.substr (0, channel_id_end);
            const std::string mid = sc_ids.substr (channel_id_end);

            ret.channel_id = std::stoull (cid);
            ret.msg_id = std::stoull (mid);
        }
    else
        {
            // sc_ids == mid
            ret.msg_id = std::stoull (sc_ids);
        }

    return ret;
}

dpp::slashcommand
get_register_obj (const dpp::snowflake &sha_id)
{
    dpp::command_option channel_opt (dpp::co_channel, "to",
                                     "Channel to send the message");

    using ct = dpp::channel_type;
    channel_opt.channel_types
        = { ct::CHANNEL_TEXT, ct::CHANNEL_ANNOUNCEMENT,
            ct::CHANNEL_ANNOUNCEMENT_THREAD,
            /* ct::CHANNEL_FORUM, ct::CHANNEL_MEDIA, */
            ct::CHANNEL_PRIVATE_THREAD, ct::CHANNEL_PUBLIC_THREAD,
            ct::CHANNEL_VOICE, ct::CHANNEL_STAGE };

    return dpp::slashcommand (
               "say", "Just say it! Ima say it if ur too scared to say it",
               sha_id)
        .add_option (dpp::command_option (dpp::co_string, "content",
                                          "Content of the message", true))
        .add_option (channel_opt);
}

void
slash_run (const dpp::slashcommand_t &event)
{
    std::thread t ([event] () {
        thread_manager::DoneSetter tmds;

        std::string content;
        dpp::snowflake to_channel;
        get_inter_param (event, "content", &content);
        get_inter_param (event, "to", &to_channel);

        if (content.empty ())
            return event.reply ("Empty? so nothing?");

        dpp::message sndmsg (content);

#ifdef HAS_MESSAGE_INTENT
        event.thinking ();

        msg_source_t msrc = parse_msg_source (content);

        if (msrc.msg_id != 0U)
            {
                if (msrc.channel_id != 0U)
                    {
                    }
            }
#else
        if (to_channel)
            {
                auto i = event.command.resolved.channels.find (to_channel);
                if (i == event.command.resolved.channels.end ())
                    return event.reply (
                        "`[ERROR]` Unable to retrieve channel");

                sndmsg = dpp::message (i->first, content);

                event.thinking (
                    false,
                    [sndmsg, event] (
                        const dpp::confirmation_callback_t &thinking_res) {
                        if (thinking_res.is_error () || !event.from
                            || !event.from->creator)
                            return;

                        event.from->creator->message_create (
                            sndmsg, [event] (const dpp::confirmation_callback_t
                                                 &create_res) {
                                if (create_res.is_error ())
                                    // !TODO: actually check musicat and member
                                    // permission before sending
                                    return event.edit_response (
                                        "Failed sending message, please check "
                                        "my permission in the target channel");

                                event.edit_response ("Said");
                            });
                    });
            }
        else
            event.reply (sndmsg);

#endif
    });

    thread_manager::dispatch (t);
}
} // musicat::command::say
