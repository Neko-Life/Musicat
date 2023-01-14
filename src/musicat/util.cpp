#include "musicat/util.h"

namespace musicat
{
namespace util
{

std::string
join (const bool join, const std::string &str, const std::string &join_str)
{
    return join ? str + join_str : str;
}

} // util
} // musicat
