#ifndef MUSICAT_UTIL_FS_H
#define MUSICAT_UTIL_FS_H

#include <string>

namespace musicat::util::fs
{

// terminates when unable to ensure directory
// returns true if path got created
// returns false if path already exist
bool ensure_dir (const std::string &path);

// returns true if fpath regular file exists
bool file_exists (const std::string &fpath);

} // musicat::util::fs

#endif // MUSICAT_UTIL_FS_H
