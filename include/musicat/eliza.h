#ifndef MUSICAT_ELIZA_H
#define MUSICAT_ELIZA_H

#include <dpp/dpp.h>

namespace musicat::eliza
{

int init ();
std::string ask (const std::string &userinput);

void handle_message_create (const dpp::message_create_t &event);

} // musicat::eliza

#endif // MUSICAT_ELIZA_H
