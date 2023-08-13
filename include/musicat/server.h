#ifndef MUSICAT_SERVER_H
#define MUSICAT_SERVER_H

#include <mutex>
#include <string>

namespace musicat
{
namespace server
{
enum ws_req_t
{
    bot_info = 1,
    server_list = 2,
    oauth_req = 3,
    invite_req = 4,
};

enum ws_event_t
{
    oauth = 1,
};

struct SocketData
{
};

// always lock this whenever calling publish()
extern std::mutex ns_mutex;

bool get_running_state ();

// this will be handled on `message` event in the client,
// so be sure to use helper function to construct
// whatever data you want to send
int publish (const std::string &topic, const std::string &message);

// shutdown server, returns 0 on success, else 1
int shutdown ();

int run ();

} // server
} // musicat

#endif // MUSICAT_SERVER_H
