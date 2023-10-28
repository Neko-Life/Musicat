#include "musicat/server/response.h"

namespace musicat::server::response
{

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

} // musicat::server::response
