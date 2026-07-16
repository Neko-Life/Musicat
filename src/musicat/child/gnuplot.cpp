#include "musicat/child/gnuplot.h"
#include "musicat/child.h"

namespace musicat::child::gnuplot
{

std::string
get_gnuplot_out_fifo_path (const std::string &id)
{
    return get_local_dir_ensure () + id + ".gnuplotout";
}

} // musicat::child::gnuplot
