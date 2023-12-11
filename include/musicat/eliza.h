#ifndef MUSICAT_ELIZA_H
#define MUSICAT_ELIZA_H

#include "ofxEliza.h"
#include <dpp/dpp.h>

namespace musicat::eliza
{

int check ();

void handle_message_create (const dpp::message_create_t &event);

} // musicat::eliza

#endif // MUSICAT_ELIZA_H
