#ifndef filters_hh_INCLUDED
#define filters_hh_INCLUDED

#include <boost/regex.hpp>
#include "display_buffer.hh"

namespace Kakoune
{

void colorize_regex(DisplayBuffer& display_buffer,
                    const boost::regex& ex, Color color);

}

#endif // filters_hh_INCLUDED
