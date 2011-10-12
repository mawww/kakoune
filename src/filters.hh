#ifndef filters_hh_INCLUDED
#define filters_hh_INCLUDED

#include <boost/regex.hpp>
#include "display_buffer.hh"

namespace Kakoune
{

void colorize_regex(DisplayBuffer& display_buffer,
                    const boost::regex& ex, Color color);

void colorize_cplusplus(DisplayBuffer& display_buffer);
void colorize_cplusplus(DisplayBuffer& display_buffer);
void expand_tabulations(DisplayBuffer& display_buffer);

}

#endif // filters_hh_INCLUDED
