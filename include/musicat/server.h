#ifndef MUSICAT_SERVER_H
#define MUSICAT_SERVER_H

#include <mutex>
#include <string>
#include <uWebSockets/src/App.h>

#define SERVER_WITH_SSL false
#define BOT_AVATAR_SIZE 480

namespace musicat::server
{

// always lock this whenever updating states
// EXTERN_VARIABLE
extern std::mutex ns_mutex;

bool get_running_state ();

// this will be handled on `message` event in the client,
// so be sure to use helper function to construct
// whatever data you want to send
int publish (const std::string &topic, const std::string &message);

// shutdown server, returns 0 on success, else 1
int shutdown ();

int run ();

} // musicat::server

#endif // MUSICAT_SERVER_H
