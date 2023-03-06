#ifndef MUSICAT_SERVER_H
#define MUSICAT_SERVER_H

#include <mutex>

namespace musicat
{
namespace server
{
struct SocketData
{
};

// always lock this whenever calling publish()
extern std::mutex ns_mutex;

bool get_running_state ();

// this will be handled on `message` event in the client,
// so be sure to use helper function to construct
// whatever data you want to send
int
publish (const std::string& topic, const std::string& message);

int run ();

} // server
} // musicat

#endif // MUSICAT_SERVER_H
