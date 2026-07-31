#include "musicat/server/response.h"
#include "musicat/musicat.h"
#include "musicat/server.h"
#include "musicat/server/middlewares.h"
#include <cstdint>
#include <mutex>
#include <vector>

namespace musicat::server::response
{

end_t::end_t () : status (http_status_t.OK_200), res (NULL) { init (); }

end_t::end_t (APIResponse *_res) : status (http_status_t.OK_200), res (_res) { init (); }

static uint64_t active_end_count = 0;
static std::mutex active_end_count_m;

static std::vector<APIResponse *> aborted_res;
static std::mutex aborted_res_m;

static bool
is_aborted (APIResponse *res)
{
    if (!res)
        return true;

    auto i = aborted_res.begin ();
    while (i != aborted_res.end ())
        {
            if (*i == res)
                return true;
            i++;
        }

    return false;
}

bool
end_t::is_aborted ()
{
    return ::musicat::server::response::is_aborted (res);
}

void
end_t::mark_aborted (APIResponse *res)
{
    std::lock_guard lk (aborted_res_m);
    aborted_res.push_back (res);
}

void
end_t::init ()
{
    std::lock_guard lk (active_end_count_m);
    active_end_count++;
}

static void
clear_aborted ()
{
    const bool debug = get_debug_state ();

    std::lock_guard lk (active_end_count_m);
    active_end_count--;
    if (debug)
        {
            std::lock_guard lk2 (aborted_res_m);
            std::cerr << "[server::response::clear_aborted] aborted_res.size(" << aborted_res.size () << ")\n";
        }
    if (active_end_count)
        return;

    std::lock_guard lk2 (aborted_res_m);
    aborted_res.clear ();
    if (debug)
        std::cerr << "[server::response::clear_aborted] aborted_res cleared\n";
}

end_t::~end_t ()
{

    if (is_aborted () || !status)
        {
            clear_aborted ();
            return;
        }

    res->writeStatus (status);

    if (!headers.empty ())
        middlewares::write_headers (res, headers);

    if (response.empty ())
        {
            res->end ();
            res = NULL;
            clear_aborted ();
            return;
        }

    res->end (response);
    res = NULL;
    clear_aborted ();
}

header_v_t::iterator
end_t::get_header_iterator (std::string_view _key)
{
    auto i = headers.begin ();
    while (i != headers.end ())
        {
            if (i->first == _key)
                break;

            i++;
        }

    return i;
}

std::pair<std::string, std::string>
end_t::get_header (std::string_view _key)
{
    auto i = get_header_iterator (_key);
    if (i != headers.end ())
        return *i;

    return { "", "" };
}

end_t *
end_t::add_header (std::string_view _key, std::string_view _value)
{
    headers.emplace_back (_key, _value);

    return this;
}

end_t *
end_t::set_header (std::string_view _key, std::string_view _value)
{
    auto i = get_header_iterator (_key);
    if (i != headers.end ())
        i->second = _value;
    else
        add_header (_key, _value);

    return this;
}

std::pair<std::string, std::string>
end_t::remove_header (std::string_view _key)
{
    auto i = get_header_iterator (_key);
    if (i == headers.end ())
        return { "", "" };

    auto ret = *i;
    headers.erase (i);
    return ret;
}

end_t *
end_t::set_content_type_header (std::string_view _ct)
{
    set_header (header_key_t.content_type, _ct);

    return this;
}

end_t *
end_t::remove_content_type_header ()
{
    remove_header (header_key_t.content_type);

    return this;
}

end_t *
end_t::set_content_type_json ()
{
    set_content_type_header (content_type_t.json);

    return this;
}

defer_end_t::defer_end_t () : endres (NULL) {};

defer_end_t::defer_end_t (end_t &_endres) : endres (&_endres) {};

defer_end_t::~defer_end_t ()
{
    if (!endres)
        return;

    if (endres->is_aborted ())
        return;

    APIResponse *res = endres->res;
    endres->res = NULL;

    const char *rstatus = endres->status;
    const header_v_t rheaders = endres->headers;
    const std::string rresponse = endres->response;

    defer (
        [rstatus, rheaders, rresponse, res] ()
            {
                response::end_t e (res);
                e.status = rstatus;
                e.headers = rheaders;
                e.response = rresponse;
            });
}

// ================================================================================

nlohmann::json
payload (const nlohmann::json &data)
{
    return { { "success", true }, { "data", data } };
}

nlohmann::json
error (error_code_e code, std::string_view message)
{
    return { { "success", false }, { "data", { { "code", code }, { "message", message } } } };
}

} // musicat::server::response
