#ifdef MUSICAT_WITH_PYTHON

#ifndef MUSICAT_YTDLP_H
#define MUSICAT_YTDLP_H

#include "nlohmann/json.hpp"

namespace musicat::ytdlp
{

namespace managed
{

void on_thread_done ();

} // namespace managed

void set_init_params (const std::string &_program_name, const std::string &_pwd, const std::string &_lib_path);
int fetch (const std::string &query, int max_entries, nlohmann::json &out, const std::string &outfile = "");

} // namespace musicat::ytdlp

#endif // MUSICAT_YTDLP_H

#endif // MUSICAT_WITH_PYTHON
