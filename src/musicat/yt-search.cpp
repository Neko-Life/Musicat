/* Very simple YouTube track searching function. Made with love by Neko Life :heart: */

#include <sstream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <curlpp/Infos.hpp>
#include "nlohmann/json.hpp"
#include "musicat/encode.h"
#include "musicat/yt-search.h"

namespace yt_search {
	bool transverse(nlohmann::json& j, const std::initializer_list<const char*> path) {
		for (auto& i : path)
		{
			j = j.size() ? (j.is_array() ? j.at(strtoul(i, NULL, 10)) : j.value<nlohmann::json>(i, {})) : nlohmann::json();
			if (j.is_null())
			{
				fprintf(stderr, "[YT_SEARCH_TRANSVERSE] j[%s].is_null\n", i);
				return false;
			}
		}
		return true;
	}

	void loop_json(nlohmann::json& d, std::function<void (nlohmann::json::iterator)> cb) {
		for (nlohmann::json::iterator i = d.begin(); i != d.end(); i++)
		{
			if (i->is_null()) continue;
			cb(i);
		}
	}

	long status_code(const std::string& url) {
		std::ostringstream stream;
		curlpp::Cleanup curl_cleanup;

		curlpp::Easy req;

		req.setOpt(curlpp::options::Url(url));
		req.setOpt(curlpp::options::Header("User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:98.0) Gecko/20100101 Firefox/98.0"));
		req.setOpt(curlpp::options::WriteStream(&stream));
		
		try
		{
			req.perform();
		}
		catch (const curlpp::LibcurlRuntimeError& e)
		{
			fprintf(stderr, "[ERROR STATUS_CODE] LibcurlRuntimeError(%d): %s\n", e.whatCode(), e.what());
			return -1;
		}

		return curlpp::infos::ResponseCode::get(req);
	}

	using json = nlohmann::json;

	std::string YTrack::snippetText() const {
		std::string res = "";
		json d = raw;

		if (transverse(d, { "detailedMetadataSnippets", "0", "snippetText", "runs" }))
			if (d.size() && d.is_array())
				loop_json(d, [&res](nlohmann::json::iterator i) {
					std::string text = i->value<std::string>("text", "");

					if (i->value<bool>("bold", false))
						res += "**" + text + "**";
					else res += text;
				});

		return res;
	}

	std::string YTrack::id() const {
		return raw.is_null() ? "" : raw.value<std::string>("videoId", "");
	}

	std::string YTrack::url() const {
		return std::string("https://www.youtube.com/watch?v=") + id();
	}

	std::string YTrack::trackingParams() const {
		return raw.is_null() ? "" : raw.value<std::string>("trackingParams", "");
	}

	int YTrack::get_thumb(nlohmann::json& d, const std::string& url, const bool debug) {
		int status = 0;

		bool exist = false;
		loop_json(d, [&exist, &url](nlohmann::json::iterator i) {
			const std::string i_url = i->value<std::string>("url", "");
			if (url == i_url) exist = true;
		});

		if (!exist) {
			// if (status_code(url) == 200L) {
			// 	nlohmann::json obj;
			// 	obj["url"] = url;
			// 	d.push_back(obj);
			// 	this->raw["thumbnail"]["thumbnails"] = d;
			// 	if (debug) printf("[THUMB HQ_MAX] Inserted thumb: '%s'\n", url.c_str());
			// }
			// else status = -1;
			nlohmann::json obj;
			obj["url"] = url;
			d.push_back(obj);
		}

		return status;
	}

	int YTrack::load_hq_thumb(nlohmann::json& d) {
		const bool debug = true; // get_debug_state
		const std::string id = this->id();

		std::string url_max_res("http://i3.ytimg.com/vi/");
		url_max_res += id + "/maxresdefault.jpg";

		std::string url_hq_res("http://i3.ytimg.com/vi/");
		url_hq_res += id + "/hqdefault.jpg";

		// int result_get = get_thumb(d, url_max_res, debug);
		// if (result_get != 0) result_get = get_thumb(d, url_hq_res, debug);

		int result_get = get_thumb(d, url_hq_res, debug);
		return result_get;
	}

	std::vector<YThumbnail> YTrack::thumbnails() {
		std::vector<YThumbnail> res;
		json d = raw;

		if (transverse(d, { "thumbnail", "thumbnails" })) {		
			if (d.size() && d.is_array()) {
				// get_debug_state
				if (load_hq_thumb(d) != 0 && true) fprintf(stderr, "[LOAD_HQ_THUMB] Failed to get hq thumb\n");
				loop_json(d, [&res](nlohmann::json::iterator i) {
					res.push_back({
						i->value<int>("height", 0),
						i->value<std::string>("url",""),
						i->value<int>("width", 0)
					});
				});
			}
		}

		return res;
	}

	YThumbnail YTrack::bestThumbnail() {
		return thumbnails().back();
	}

	// Track length
	// TODO: Parse to ms
	std::string YTrack::length() const {
		json d = raw;

		return transverse(d, { "lengthText" }) ? d.value<std::string>("simpleText", "") : "";
	}

	YChannel YTrack::channel() const {
		json d = raw;

		if (!transverse(d, { "longBylineText","runs","0" })) return {};

		json m = d;

		return {
			d.value<std::string>("text",""),
				transverse(m, { "navigationEndpoint","commandMetadata","webCommandMetadata","url" })
					? std::string("https://www.youtube.com") + m.get<std::string>()
					: ""
		};
	}

	std::string YTrack::title() const {
		json d = raw;

		if (!transverse(d, { "title","runs","0","text" }))
		{
			d = raw;

			if (!transverse(d, { "title", "simpleText" })) return "";
		}

		return d.get<std::string>();
	}

	// This is interesting
	std::string YSearchResult::estimatedResults() const {
		return raw.is_null() ? "" : raw.value<std::string>("estimatedResults", "");
	}

	std::vector<YTrack> YSearchResult::trackResults() const {
		std::vector<YTrack> res;
		json d = raw;

		transverse(d, { "contents",
				"twoColumnSearchResultsRenderer",
				"primaryContents",
				"sectionListRenderer",
				"contents",
				"0",
				"itemSectionRenderer",
				"contents" });

		if (!d.size() || (d.is_array() && d.at(0).is_null())) return res;

		for (size_t i = 0; i < d.size(); i++)
		{
			json data = d;

			if (!transverse(data, { std::to_string(i).c_str(),"videoRenderer" })) continue;

			res.push_back({
					data
					});
		}

		return res;
	}

	YSearchResult search(std::string search) {
		YSearchResult ret;
		get_data(std::string("https://www.youtube.com/results?search_query=") + encodeURIComponent(search), &ret);
		return ret;
	}
}
