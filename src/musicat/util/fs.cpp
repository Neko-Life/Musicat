#include <filesystem>
#include <iostream>
#include <string>
#include <sys/stat.h>

namespace musicat::util::fs
{

bool
ensure_dir (const std::string &path)
{
    struct stat sb;
    if (stat (path.c_str (), &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFDIR)
        return false;

    if (std::filesystem::create_directory (path))
        return true;

    std::cerr << "[util::fs::ensure_dir FATAL] Unable to create directory: " << path << "\n";
    std::terminate ();
}

bool
file_exists (const std::string &fpath)
{
    struct stat sb;
    if (stat (fpath.c_str (), &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFREG)
        return true;
    return false;
}

} // musicat::util::fs
