#ifndef MUSICAT_SERVER_RESPONSE_H
#define MUSICAT_SERVER_RESPONSE_H

#include "musicat/function_macros.h"
#include "musicat/server.h"
#include "nlohmann/json.hpp"

namespace musicat::server::response
{

enum error_code_e
{
    ERROR_CODE_NOTHING
};

struct end_t
{
    const char    *status;
    header_v_t     headers;
    std::string    response;
    APIResponse   *res;

    end_t            ();

    DELETE_COPY_MOVE_CTOR(end_t);

    explicit end_t   (APIResponse *_res);
    ~end_t           ();

    [[nodiscard]] header_v_t::iterator   get_header_iterator (std::string_view  _key);
    std::pair<std::string, std::string>  get_header          (std::string_view  _key);
    end_t                               *add_header          (std::string_view  _key, std::string_view  _value);
    end_t                               *set_header          (std::string_view  _key, std::string_view  _value);
    std::pair<std::string, std::string>  remove_header       (std::string_view  _key);

    end_t                               *set_content_type_header    (std::string_view  _ct);
    end_t                               *remove_content_type_header ();
    end_t                               *set_content_type_json      ();

};

struct defer_end_t
{
    end_t *     endres;

    defer_end_t ();

    DELETE_COPY_MOVE_CTOR(defer_end_t);

    explicit defer_end_t   (end_t &_endres);
    ~defer_end_t           ();
};

nlohmann::json payload     (const nlohmann::json &data);
nlohmann::json error       (error_code_e code, std::string_view message);

} // musicat::server::response

#endif // MUSICAT_SERVER_RESPONSE_H
