#ifndef MUSICAT_CHILD_GNUPLOT_H
#define MUSICAT_CHILD_GNUPLOT_H

#include <string>

namespace musicat::child::gnuplot
{

std::string get_gnuplot_out_fifo_path (const std::string &id);

} // musicat::child::gnuplot

#endif // MUSICAT_CHILD_GNUPLOT_H
