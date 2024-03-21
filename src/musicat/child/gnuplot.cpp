#include "musicat/child/gnuplot.h"

namespace musicat::child::gnuplot
{

std::string
get_gnuplot_out_fifo_path (const std::string &id)
{
    return std::string ("/tmp/musicat.") + id + ".gnuplotout";
}

} // musicat::child::gnuplot
