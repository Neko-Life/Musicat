#ifndef NEKOS_BEST_H
#define NEKOS_BEST_H

#include "nlohmann/json.hpp"
#include <ctime>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nekos_best
{

static const std::string API_VERSION = "v2"; // specify api version
static const std::string BASE_URL
    = "https://nekos.best/api/"; // base url for request

// these list are just for reference and unused anywhere, the API might provide
// more updated endpoints without notice
static const std::vector<std::string> PNG_CATEGORIES
    = { "husbando", "kitsune", "neko",
        "waifu" }; // list of category with png format
static const std::vector<std::string> GIF_CATEGORIES = {
        // list of category with gif format
        "baka",  "bite",     "blush", "bored",    "cry",      "cuddle",
        "dance", "facepalm", "feed",  "handhold", "happy",    "highfive",
        "hug",   "kick",     "iss",   "laugh",    "pat",      "poke",
        "pout",  "punch",    "shoot", "shrug",    "slap",     "sleep",
        "smile", "smug",     "stare", "think",    "thumbsup", "tickle",
        "wave",  "wink",     "yeet",
};

/**
 * @brief Known and available image format
 */
enum image_format
{
        // default value for function parameter
        if_none = 0,
        // PNG
        if_png = 1,
        // GIF
        if_gif = (1 << 1),
};

/**
 * @brief Response container
 */
struct Response
{
        // response status, you can use this to determine whether a request
        // succeeded
        long status_code;

        // header size contained in response stream
        long header_size;

        // response headers, a key value pair
        std::map<std::string, std::string> headers;

        // raw json response, can contain error messages,
        // probably want to add a wrapper for that but no doc to be found.
        // It will be as it is for now.
        nlohmann::json raw_json;
};

// !TODO: error struct, get_error func

/**
 * @brief Endpoint specification struct
 */
struct EndpointSpec
{
        std::string name;
        std::string min;
        std::string max;
        image_format format;
};

using endpoint_map = std::map<std::string, EndpointSpec>;

/**
 * @brief Metadata struct, for gif format usually only anime_name and url are
 * populated, whereas anime_name isn't populated if the image were a png format
 */
struct Meta
{
        std::string artist_href;
        std::string artist_name;
        std::string source_url;
        std::string url;
        std::string anime_name;
};

/**
 * @brief Query result struct
 */
struct QueryResult
{
        std::vector<Meta> results;
};

/**
 * @brief Rate limit info struct
 */
struct RateLimitInfo
{
        // endpoint name
        std::string endpoint;
        std::tm rate_limit_at;
        std::tm rate_limit_reset_at;
};

/**
 * @brief Namespace mutex for thread safety.
 * You do not want any internal race condition if you do threading with this
 * library. If you do use threads (eg. for async neko fetch), always lock this
 * whenever you call any of the function this library provides.
 */
extern std::mutex ns_mutex;

/**
 * @brief Initializes client, call this once to cache endpoints data from the
 * API.
 *
 * An unitialized client can't get random endpoint if any of the fetch
 * omittable param is omitted. This function simply calls
 * get_available_endpoints() once to populate endpoints cache for randomizing
 * omitted fetch params.
 *
 * Be careful with unitialized client, you might encounter undefined behavior
 * which can cause segfault if any of the fetch param is omitted, simply due to
 * the nature of unitialized variable in C++.
 */
void init ();

/**
 * @brief Get API base url
 */
std::string get_base_url ();

/**
 * @brief Convert image_format flag to string
 */
std::string get_str_format (image_format format);

/**
 * @brief Convert format string to image_format flag
 * @returns image_format default to if_none if str doesn't match any flag
 */
image_format parse_str_format (std::string str);

/**
 * @brief Get API list of available endpoint
 */
endpoint_map get_available_endpoints ();

/**
 * @brief Get last API response, emptying the response cache.
 * 	You should copy it somewhere else if you were gonna keep it.
 */
Response get_last_request_response ();

/**
 * @brief Check whether endpoint/url is currently ratelimited
 */
bool is_rate_limited (const std::string &endpoint_or_url);

/**
 * @brief Get rate limit info struct for endpoint or url.
 * 	Returns empty struct if nothing found.
 */
RateLimitInfo get_rate_limit_info (const std::string &endpoint_or_url);

/**
 * @brief Fetch to a category endpoint
 * @param category will pick one randomly if omitted
 * @param amount default to 1
 */
QueryResult fetch (const std::string &category = "", const int amount = 1);

/**
 * @brief Fetch single image directly
 * @param category will pick one randomly if omitted
 * @param filename image filename, variable digit of number as string, will
 *                      pick one randomly if omitted
 * @param format will pick one randomly if omitted
 * @param download_stream pointer to response buffer to store both header and
 *                      raw response
 */
Meta fetch_single (const std::string &category = "",
                   const std::string &filename = "",
                   const image_format format = if_none,
                   std::ostringstream *download_stream = nullptr);

/**
 * @brief Search for image
 * @param query search query
 * @param type will pick one randomly if omitted
 * @param category image category
 * @param amount result amount, default to 1
 */
QueryResult search (const std::string &query,
                    const image_format format = if_none,
                    const std::string &category = "", const int amount = 1);
} // nekos_best

#endif // NEKOS_BEST_H
