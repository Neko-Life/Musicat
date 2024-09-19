#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/routes/oauth.h"

namespace musicat::server::routes
{

void
get_invite (APIResponse *res, APIRequest *req)
{
    return oauth::get_oauth_handler (res, req, get_oauth_invite);
}

} // musicat::server::routes
