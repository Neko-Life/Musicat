#include "musicat/server/response.h"

namespace musicat::server::response
{
#ifndef MUSICAT_NO_SERVER

nlohmann::json
payload (const nlohmann::json &data)
{
    return { { "success", true }, { "data", data } };
}

nlohmann::json
error (error_code_e code, const std::string &message)
{
    return { { "success", false },
             { "data", { { "code", code }, { "message", message } } } };
}

#endif
} // musicat::server::response
