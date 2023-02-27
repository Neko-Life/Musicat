#ifndef MUSICAT_SERVER_H
#define MUSICAT_SERVER_H

namespace musicat
{
namespace server
{
struct SocketData
{
};

bool get_running_state ();
/*
uWS::App *
get_app_ptr ()
*/
int run ();

} // server
} // musicat

#endif // MUSICAT_SERVER_H
