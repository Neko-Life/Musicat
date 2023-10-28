#ifndef MUSICAT_SERVER_RESPONSE_H
#define MUSICAT_SERVER_RESPONSE_H

#include "nlohmann/json.hpp"

namespace musicat::server::response
{

enum error_code_e
{
    ERROR_CODE_NOTHING
};

nlohmann::json payload (const nlohmann::json &data);

nlohmann::json error (error_code_e code, const std::string &message);

} // musicat::server::response

#endif // MUSICAT_SERVER_RESPONSE_H
