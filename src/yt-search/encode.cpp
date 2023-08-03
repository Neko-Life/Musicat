#include <string>
#include <regex>
#include "yt-search/encode.h"

std::string decodeURIComponent(std::string encoded) {

    std::string decoded = encoded;
    std::smatch sm;
    std::string haystack;

    int dynamicLength = decoded.size() - 2;

    if (decoded.size() < 3) return decoded;

    for (int i = 0; i < dynamicLength; i++)
    {

        haystack = decoded.substr(i, 3);

        if (std::regex_match(haystack, sm, std::regex("%[0-9A-F]{2}")))
        {
            haystack = haystack.replace(0, 1, "0x");
            std::string rc = { (char)std::stoi(haystack, nullptr, 16) };
            decoded = decoded.replace(decoded.begin() + i, decoded.begin() + i + 3, rc);
        }

        dynamicLength = decoded.size() - 2;

    }

    return decoded;
}

std::string encodeURIComponent(std::string decoded)
{

    std::ostringstream oss;
    std::regex r("[!'\\(\\)*-.0-9A-Za-z_~]");

    for (char& c : decoded)
    {
        if (std::regex_match((std::string) { c }, r))
        {
            oss << c;
        }
        else
        {
            oss << "%" << std::uppercase << std::hex << (0xff & c);
        }
    }
    return oss.str();
}
