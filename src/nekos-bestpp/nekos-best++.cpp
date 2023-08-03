#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include <curlpp/Easy.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>

#include "nekos-best++.hpp"
#include "nlohmann/json.hpp"

namespace nekos_best
{
enum _req_flag
{
        NONE = 0,
        NOBODY = 1,
};

std::mutex ns_mutex;

endpoint_map _endpoints_c = {};
Response _resp_c;

std::map<std::string, RateLimitInfo> _rate_limits_c = {};

int
_get_random_number ()
{
        srand (std::chrono::high_resolution_clock::now ()
                   .time_since_epoch ()
                   .count ());

        return rand ();
}

image_format
_get_category_format (const std::string &category)
{
        const auto find = _endpoints_c.find (category);

        if (find == _endpoints_c.end ())
                return if_none;

        return find->second.format;
}

image_format
_get_random_format ()
{
        const int random = _get_random_number () & 1;

        switch (random)
                {
                case 1:
                        return if_png;
                case 0:
                        return if_gif;
                }

        // this can never possibly happen! but eh
        // or should we just return png instead?
        return if_none;
}

std::vector<std::string>
_get_endpoints_with_format (const image_format format)
{
        std::vector<std::string> endpoints = {};

        for (auto i = _endpoints_c.begin (); i != _endpoints_c.end (); i++)
                {
                        if (i->second.format == format)
                                endpoints.push_back (i->first);
                }

        return endpoints;
}

EndpointSpec
_get_random_endpoint (const image_format format)
{
        const auto available = _get_endpoints_with_format (format);

        const size_t a_size = available.size ();
        if (!a_size)
                {
                        fprintf (stderr,
                                 "[nekos-best++ ERROR] No available endpoint "
                                 "for format: "
                                 "\"%s\"\n",
                                 get_str_format (format).c_str ());
                        return { "", "", "", if_none };
                }

        const int random = _get_random_number () % a_size;

        return _endpoints_c.at (*(available.begin () + random));
}

std::string
_get_random_filename (const EndpointSpec &endpoint)
{
        const size_t pad = endpoint.max.length ();
        const int min = atoi (endpoint.min.c_str ());
        const int max = atoi (endpoint.max.c_str ());

        const int div_by = (max + 1 - min);

        if (pad && div_by > 0)
                {
                        const int random
                            = (_get_random_number () % div_by) + min;

                        std::string result = std::to_string (random);
                        const size_t res_length = result.length ();

                        if (pad < res_length)
                                {
                                        fprintf (stderr,
                                                 "[nekos-best++ ERROR] "
                                                 "Unexpected error, base "
                                                 "string is longer than total "
                                                 "length\n");
                                        return "";
                                }

                        for (size_t i = 0; i < (pad - res_length); i++)
                                {
                                        result = "0" + result;
                                }

                        return result;
                }
        else
                {
                        fprintf (stderr,
                                 "[nekos-best++ ERROR] API error, unexpected "
                                 "endpoint "
                                 "spec: min(%s), max(%s), pad(%ld)\n",
                                 endpoint.min.c_str (), endpoint.max.c_str (),
                                 pad);
                        return "";
                }
}

void
_set_last_response (Response *resp)
{
        _resp_c.headers = resp && resp->headers.size ()
                              ? resp->headers
                              : std::map<std::string, std::string> ();
        _resp_c.header_size
            = resp && resp->header_size ? resp->header_size : 0L;
        _resp_c.status_code
            = resp && resp->status_code > 0L ? resp->status_code : 0L;
        _resp_c.raw_json = resp && resp->raw_json.size () ? resp->raw_json
                                                          : nlohmann::json ();
}

Response
_get_last_response (const bool clear = false)
{
        Response cpy = _resp_c;

        if (clear)
                _set_last_response (nullptr);

        return cpy;
}

void
_fill_meta (Meta &meta, nlohmann::json &json)
{
        meta.artist_href = json.value ("artist_href", "");
        meta.artist_name = json.value ("artist_name", "");
        meta.source_url = json.value ("source_url", "");
        meta.url = json.value ("url", "");
        meta.anime_name = json.value ("anime_name", "");
}

void
_populate_download_meta (Meta &data, Response &response)
{
        const auto not_found = response.headers.end ();

        const auto res_artist_href = response.headers.find ("artist_href");
        data.artist_href = res_artist_href != not_found
                               ? curlpp::unescape (res_artist_href->second)
                               : "";

        const auto res_artist_name = response.headers.find ("artist_name");
        data.artist_name = res_artist_name != not_found
                               ? curlpp::unescape (res_artist_name->second)
                               : "";

        const auto res_source_url = response.headers.find ("source_url");
        data.source_url = res_source_url != not_found
                              ? curlpp::unescape (res_source_url->second)
                              : "";

        const auto res_anime_name = response.headers.find ("anime_name");
        data.anime_name = res_anime_name != not_found
                              ? curlpp::unescape (res_anime_name->second)
                              : "";
}

// internal use, returns Response object
Response
_request (const std::string &req_url, const std::string &endpoint = "",
          _req_flag _flag = _req_flag::NONE,
          std::ostringstream *download_stream = nullptr)
{
        std::string using_endpoint = endpoint.length () ? endpoint : req_url;
        if (using_endpoint.length ())
                {
                        if (is_rate_limited (using_endpoint))
                                {
                                        fprintf (stderr,
                                                 "[nekos-best++ ERROR] You're "
                                                 "still rate limited!\n");

                                        return {};
                                }
                }

        using namespace curlpp;

        std::ostringstream result_stream;

        Easy request;

        // set header
        request.setOpt (Options::Header ("user-agent: nekos.best++"));
        request.setOpt (Options::WriteStream (
            download_stream == nullptr ? &result_stream : download_stream));

        if (_flag & _req_flag::NOBODY)
                {
                        request.setOpt (Options::NoBody (1));
                }

        request.setOpt (Options::Url (req_url));

        // perform the request
        request.perform ();
        if (download_stream)
                result_stream = std::ostringstream (download_stream->str ());

        const auto res_code = Infos::ResponseCode::get (request);

        if (res_code != 200L)
                {
                        fprintf (stderr,
                                 "[nekos-best++ WARN] Status code: %ld\n",
                                 res_code);
                }

        const auto header_size = Infos::HeaderSize::get (request);
        const std::string res_str = result_stream.str ();

        std::map<std::string, std::string> headers = {};

        // parse headers
        {
                std::istringstream str_stream (
                    res_str.substr (0, header_size));
                for (std::string line; std::getline (str_stream, line);)
                        {
                                const size_t pos = line.find (": ");

                                if (pos != std::string::npos)
                                        {
                                                const std::string key
                                                    = line.substr (0, pos);
                                                const std::string value_with_cr
                                                    = line.substr (pos + 2);
                                                const std::string value
                                                    = value_with_cr.substr (
                                                        0,
                                                        value_with_cr.length ()
                                                            - 1);

                                                headers.insert (
                                                    std::make_pair (key,
                                                                    value));
                                        }
                        }
        }

        if (header_size > res_str.length ())
                {
                        fprintf (
                            stderr,
                            "[nekos-best++ ERROR] Failed getting response. "
                            "Header size > response length\n");
                        return _resp_c;
                }

        const std::string res = res_str.substr (header_size);
        bool parse = true;

        if (res_code != 200L)
                {
                        fprintf (stderr,
                                 "[nekos-best++ WARN] Response JSON:\n%s\n",
                                 res.c_str ());
                }

        if (!res.length () || download_stream != nullptr)
                {
                        if (!(_flag & _req_flag::NOBODY))
                                fprintf (stderr, "[nekos-best++ WARN] Request "
                                                 "has no result\n");
                        parse = false;
                }

        // 2022-12-08T13:08:40.0000000Z
        // %Y-%m-%dT%H:%M:%S

        // check for rate limit headers
        const auto headers_end = headers.end ();
        const auto rate_limit_remaining
            = headers.find ("x-rate-limit-remaining");

        if (rate_limit_remaining != headers_end
            && rate_limit_remaining->second == "0")
                {
                        const auto rate_limit_reset
                            = headers.find ("x-rate-limit-reset");

                        if (rate_limit_reset != headers_end)
                                {
                                        std::tm time_struct = {};
                                        time_struct.tm_isdst = -1;

                                        // parse time
                                        std::istringstream time_stream (
                                            rate_limit_reset->second);
                                        time_stream >> std::get_time (
                                            &time_struct, "%Y-%m-%dT%H:%M:%S");

                                        if (!time_stream.fail ())
                                                {

                                                        if (using_endpoint
                                                                .length ())
                                                                {
                                                                        // store
                                                                        // information
                                                                        // in
                                                                        // cache
                                                                        RateLimitInfo
                                                                            info;

                                                                        info.endpoint
                                                                            = using_endpoint;
                                                                        time_t
                                                                            tim;
                                                                        time (
                                                                            &tim);
                                                                        info.rate_limit_at
                                                                            = *(std::gmtime (
                                                                                &tim));
                                                                        info.rate_limit_reset_at
                                                                            = time_struct;

                                                                        auto
                                                                            cache
                                                                            = _rate_limits_c
                                                                                  .find (
                                                                                      using_endpoint);

                                                                        if (cache
                                                                            != _rate_limits_c
                                                                                   .end ())
                                                                                cache
                                                                                    ->second
                                                                                    = info;
                                                                        else
                                                                                _rate_limits_c
                                                                                    .insert (std::make_pair (
                                                                                        using_endpoint,
                                                                                        info));
                                                                }
                                                }
                                        else
                                                fprintf (
                                                    stderr,
                                                    "[nekos-best++ ERROR] "
                                                    "Failed to parse rate "
                                                    "limit "
                                                    "reset timestamp, "
                                                    "unexpected format: "
                                                    "'%s'\n",
                                                    rate_limit_reset->second
                                                        .c_str ());
                                }
                        else
                                fprintf (stderr, "[nekos-best++ ERROR] API "
                                                 "error, rate limited but no "
                                                 "reset timestamp. Can't "
                                                 "determine if actually rate "
                                                 "limited or not\n");
                }

        // delete info struct from cache if the endpoint no longer rate limited
        // disabled cuz bloat
        // else {
        // 	auto cache 	= _rate_limits_c.find(using_endpoint);
        // 	if (cache != _rate_limits_c.end()) _rate_limits_c.erase(cache);
        // }

        _resp_c.status_code = res_code;
        _resp_c.header_size = header_size;
        _resp_c.headers = headers;
        _resp_c.raw_json
            = parse ? nlohmann::json::parse (res) : nlohmann::json ();

        return _resp_c;
}

QueryResult
_parse_query_result (Response &response)
{
        QueryResult result;
        result.results = {};

        nlohmann::json json_res = response.raw_json;

        if (json_res.is_null () || !json_res.is_object ())
                {
                        fprintf (stderr,
                                 "[nekos-best++ ERROR] Unexpected error, JSON "
                                 "Response is "
                                 "not an object:\n%s\n",
                                 json_res.dump (2).c_str ());
                        return result;
                }

        if (json_res.size ())
                {
                        nlohmann::json array = json_res["results"];
                        if (!array.is_array ())
                                {
                                        fprintf (
                                            stderr,
                                            "[nekos-best++ ERROR] Unexpected "
                                            "error, "
                                            "'results' is not array:\n%s\n",
                                            json_res.dump (2).c_str ());
                                        return result;
                                }

                        if (!array.size ())
                                {
                                        return result;
                                }

                        for (size_t i = 0; i < array.size (); i++)
                                {
                                        nlohmann::json value = array.at (i);

                                        if (!value.is_object ())
                                                {
                                                        fprintf (
                                                            stderr,
                                                            "[nekos-best++ "
                                                            "ERROR] "
                                                            "Unexpected "
                                                            "error, "
                                                            "'value' is not "
                                                            "object:\n%s\n",
                                                            value.dump (2)
                                                                .c_str ());
                                                        continue;
                                                }

                                        if (!value.size ())
                                                continue;

                                        Meta data;
                                        _fill_meta (data, value);

                                        result.results.push_back (data);
                                }
                }

        return result;
}

void
init ()
{
        get_available_endpoints ();
}

std::string
get_base_url ()
{
        return BASE_URL + API_VERSION;
}

std::string
get_str_format (image_format format)
{
        switch (format)
                {
                case if_png:
                        return "png";
                case if_gif:
                        return "gif";
                case if_none:
                        return "";
                }

        // handle casted int
        return "";
}

image_format
parse_str_format (std::string str)
{
        // expected every letter is lowercase
        if (str == "png")
                return if_png;
        else if (str == "gif")
                return if_gif;
        return if_none;
}

Response
get_last_request_response ()
{
        return _get_last_response (true);
}

bool
is_rate_limited (const std::string &endpoint_or_url)
{
        auto cache = _rate_limits_c.find (endpoint_or_url);
        if (cache == _rate_limits_c.end ())
                return false;

        time_t current_time;
        time (&current_time);
        return std::mktime (std::gmtime (&current_time))
               < std::mktime (&cache->second.rate_limit_reset_at);
}

RateLimitInfo
get_rate_limit_info (const std::string &endpoint_or_url)
{
        auto cache = _rate_limits_c.find (endpoint_or_url);
        if (cache != _rate_limits_c.end ())
                return cache->second;
        else
                return { "", {}, {} };
}

Meta
fetch_single (const std::string &category, const std::string &filename,
              const image_format format, std::ostringstream *download_stream)
{
        Meta data;

        const size_t cat_length = category.length ();

        const image_format using_format
            = format == if_none ? (cat_length ? _get_category_format (category)
                                              : _get_random_format ())
                                : format;

        const std::string str_format = get_str_format (using_format);

        std::string random_category = "";

        EndpointSpec using_endpoint;
        bool has_endpoint = false;

        if (!cat_length)
                {
                        using_endpoint = _get_random_endpoint (using_format);
                        has_endpoint = true;
                        random_category = using_endpoint.name;
                }
        else
                {
                        const auto endpoint = _endpoints_c.find (category);

                        if (endpoint != _endpoints_c.end ())
                                {
                                        using_endpoint = endpoint->second;
                                        has_endpoint = true;
                                }
                }

        const std::string using_category
            = random_category.length () ? random_category : category;

        const size_t fn_length = filename.length ();
        if (!fn_length && !has_endpoint)
                {
                        fprintf (
                            stderr,
                            "[nekos-best++ ERROR] Unitialized client, can't "
                            "get random filename. Please call `init()` to "
                            "populate endpoints cache\n");
                }
        const std::string using_filename
            = fn_length      ? filename
              : has_endpoint ? _get_random_filename (using_endpoint)
                             : "";

        const std::string req_url
            = get_base_url () + "/" + curlpp::escape (using_category) + "/"
              + curlpp::escape (using_filename) + "." + str_format;

        const bool download = download_stream != nullptr;

        data.url = req_url;
        Response response
            = _request (req_url, using_category, download ? NONE : NOBODY,
                        download_stream);
        
        _populate_download_meta (data, response);

        return data;
}

endpoint_map
get_available_endpoints ()
{
        endpoint_map endpoints = {};
        const std::string req_url = get_base_url () + "/endpoints";

        // request
        Response response = _request (req_url);
        nlohmann::json json_res = response.raw_json;

        if (!json_res.is_object ())
                {
                        fprintf (stderr,
                                 "[nekos-best++ ERROR] Unexpected error, JSON "
                                 "Response is "
                                 "not an object:\n%s\n",
                                 json_res.dump (2).c_str ());
                        return endpoints;
                }

        if (json_res.size ())
                {
                        for (auto spec_iterator = json_res.begin ();
                             spec_iterator != json_res.end (); spec_iterator++)
                                {
                                        nlohmann::json value
                                            = spec_iterator.value ();

                                        if (!value.is_object ())
                                                {
                                                        fprintf (
                                                            stderr,
                                                            "[nekos-best++ "
                                                            "ERROR] Expected "
                                                            "JSON "
                                                            "object, "
                                                            "got:\n%s\n",
                                                            value.dump (2)
                                                                .c_str ());
                                                        continue;
                                                }

                                        if (!value.size ())
                                                continue;

                                        EndpointSpec spec;

                                        spec.name = spec_iterator.key ();
                                        spec.format = parse_str_format (
                                            value.value ("format", ""));
                                        spec.min = value.value ("min", "");
                                        spec.max = value.value ("max", "");

                                        endpoints.insert (
                                            std::make_pair (spec.name, spec));
                                }
                }

        _endpoints_c = endpoints;

        return endpoints;
}

QueryResult
fetch (const std::string &category, const int amount)
{
        const std::string using_category
            = category.length ()
                  ? category
                  : _get_random_endpoint (_get_random_format ()).name;
        std::string req_url
            = get_base_url () + "/" + curlpp::escape (using_category);

        if (amount > 1)
                {
                        req_url += "?amount=" + std::to_string (amount);
                }

        // request
        Response response = _request (req_url, using_category);

        return _parse_query_result (response);
}

QueryResult
search (const std::string &query, const image_format format,
        const std::string &category, const int amount)
{
        const std::string using_format = get_str_format (
            format == if_none ? _get_random_format () : format);
        std::string req_url = get_base_url ()
                              + "/search?query=" + curlpp::escape (query)
                              + "&type=" + using_format;

        if (category.length ())
                {
                        req_url += "&category=" + curlpp::escape (category);
                }

        if (amount > 1)
                {
                        req_url += "&amount=" + std::to_string (amount);
                }

        // request
        Response response = _request (req_url, category);

        return _parse_query_result (response);
}

Meta
download (const std::string &url, std::ostringstream *download_stream)
{
        Meta data;

        static const size_t endpoint_index = (get_base_url () + "/").length();
        std::string using_category = url.substr(endpoint_index, url.find_last_of('/') - endpoint_index);

        const bool download = download_stream != nullptr;

        data.url = url;
        Response response
            = _request (url, using_category, download ? NONE : NOBODY,
                        download_stream);
        
        _populate_download_meta (data, response);

        return data;
}
} // nekos_best
