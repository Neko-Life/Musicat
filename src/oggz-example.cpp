#include <dpp/dpp.h>
#include <iomanip>
#include <sstream>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <oggz/oggz.h>

int main(int argc, char const *argv[])
{
	/* Load an ogg opus file into memory.
	 * The bot expects opus packets to be 2 channel stereo, 48000Hz.
	 * 
	 * You may use ffmpeg to encode songs to ogg opus:
	 * ffmpeg -i /path/to/song -c:a libopus -ar 48000 -ac 2 -vn -b:a 96K /path/to/opus.ogg 
	 */
	dpp::cluster bot("token", dpp::i_default_intents | dpp::i_message_content);

        bot.on_log(dpp::utility::cout_logger());

	/* Use the on_message_create event to look for commands */
	bot.on_message_create([&bot](const dpp::message_create_t & event) {
		std::stringstream ss(event.msg.content);
		std::string command;
		ss >> command;

		/* Tell the bot to join the discord voice channel the user is on. Syntax: .join */
		if (command == ".join") {
			dpp::guild * g = dpp::find_guild(event.msg.guild_id);
			if (!g->connect_member_voice(event.msg.author.id)) {
				bot.message_create(dpp::message(event.msg.channel_id, "You don't seem to be on a voice channel! :("));
			}
		}

		/* Tell the bot to play the sound file */
		if (command == ".play") {
			dpp::voiceconn* v = event.from->get_voice(event.msg.guild_id);
			if (v && v->voiceclient && v->voiceclient->is_ready()) {
                                // load the audio file with oggz
                                OGGZ *track_og = oggz_open("/path/to/opus.ogg", OGGZ_READ);

                                if (track_og) {
                                        // set read callback, this callback will be called on packets with the serialno,
                                        // -1 means every packet will be handled with this callback
                                        oggz_set_read_callback(
                                            track_og, -1,
                                            [](OGGZ *oggz, oggz_packet *packet, long serialno,
                                               void *user_data) {
                                                    dpp::voiceconn *voiceconn = (dpp::voiceconn *)user_data;

                                                    // send the audio
                                                    voiceconn->voiceclient->send_audio_opus(packet->op.packet,
                                                                                       packet->op.bytes);

                                                    // make sure to always return 0 here, read more on oggz documentation
                                                    return 0;
                                            },
                                            // this will be the value of void *user_data
                                            (void *)v);

                                        // read loop
                                        while (v && v->voiceclient && !v->voiceclient->terminating) {
                                                // you can tweak this to whatever. Here I use BUFSIZ, defined in
                                                // stdio.h as 8192
                                                static const constexpr long CHUNK_READ = BUFSIZ * 2;

                                                const long read_bytes = oggz_read(track_og, CHUNK_READ);

                                                // break on eof
                                                if (!read_bytes)
                                                        break;
                                        }
                                } else {
                                        fprintf(stderr, "Error opening file\n");
                                }

                                // don't forget to free the memory
                                oggz_close(track_og);
			}
		}
	});
	
	/* Start bot */
	bot.start(dpp::st_wait);
	return 0;
}

// vim: sw=8 ts=8 et
