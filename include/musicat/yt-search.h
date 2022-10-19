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

		std::string snippetText() const;
		std::string id() const;
		std::string url() const;
		std::string trackingParams() const;
		std::vector<YThumbnail> thumbnails();
		YThumbnail bestThumbnail();

		// Track length
		// TODO: Parse to ms
		std::string length() const;
		YChannel channel() const;
		std::string title() const;
		int get_thumb(nlohmann::json& d, const std::string& url, const bool debug);
		int load_hq_thumb(nlohmann::json& d);
	};

	struct YSearchResult {
		nlohmann::json raw;

		// This is interesting
		std::string estimatedResults() const;
		std::vector<YTrack> trackResults() const;
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
			try
			{
				req.perform();
			}
			catch (const curlpp::LibcurlRuntimeError& e)
			{
				fprintf(stderr, "[ERROR] LibcurlRuntimeError(%d): %s\n", e.whatCode(), e.what());
				return;
			}

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

	bool transverse(nlohmann::json& j, const std::initializer_list<const char*> path);
	void loop_json(nlohmann::json& d, std::function<void (nlohmann::json::iterator)> cb);
	long status_code(const std::string& url);
}

#endif
