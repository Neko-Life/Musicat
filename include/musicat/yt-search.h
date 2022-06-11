/* Very simple YouTube track searching function. Made with love by Neko Life :heart: */

#ifndef YT_SEARCH_H
#define YT_SEARCH_H

#include <vector>
#include <string>
#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include "nlohmann/json.hpp"

namespace yt_search {
    struct YChannel {
        std::string name;
        std::string url;
    };

    struct YThumbnail {
        int height;
        std::string url;
        int width;
    };

    struct YTrack {
        nlohmann::json raw;

        std::string snippetText();
        std::string id();
        std::string url();
        std::string trackingParams();
        std::vector<YThumbnail> thumbnails();
        YThumbnail bestThumbnail();

        // Track length
        // TODO: Parse to ms
        std::string length();
        YChannel channel();
        std::string title();
    };

    struct YSearchResult {
        nlohmann::json raw;

        // This is interesting
        std::string estimatedResults();
        std::vector<YTrack> trackResults();
    };

    /**
     * @brief Get json data to fill container with
     * The container must have `raw` property with a type of nlohmann::json which stores the data
     *
     * @tparam T
     * @param url Url to get data from
     * @param container Pointer to a struct to contain the data
     * @param search String to search and get the json from
     */
    template <typename T>
    void get_data(const std::string url, T* container, const std::string search = "var ytInitialData = ") {
        using json = nlohmann::json;
        std::ostringstream os;

        curlpp::Cleanup curl_cleanup;

        curlpp::Easy req;

        req.setOpt(curlpp::options::Url(url));
        req.setOpt(curlpp::options::Header("User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:98.0) Gecko/20100101 Firefox/98.0"));
        req.setOpt(curlpp::options::WriteStream(&os));
        req.perform();

        // MAGIC INIT
        const std::string rawhttp = os.str();

        static const std::string end = "};";

        const size_t sI = rawhttp.find(search);

        if (sI == std::string::npos)
        {
            // If this getting printed to the console, the magic may be expired
            fprintf(stderr, "Not a valid youtube search query (or youtube update, yk they like to change stuffs)\nvar_start: %ld\n", sI);
            return;
        }

        const size_t eI = rawhttp.find(end, sI);
        if (eI == std::string::npos)
        {
            // If this getting printed to the console, the magic may be expired
            fprintf(stderr, "Not a valid youtube search query (or youtube update, yk they like to change stuffs)\nvar_start: %ld\nvar_end: %ld\n", sI, eI);
            return;
        }

        // MAGIC FINALIZE
        const size_t am = sI + search.length();
        const std::string tJson = rawhttp.substr(am, eI - am + 1);
        container->raw = json::parse(tJson);
    }

    /**
     * @brief Search and return track results
     *
     * @param search Query
     * @return YSearchResult
     */
    YSearchResult search(std::string search);
}

#endif
