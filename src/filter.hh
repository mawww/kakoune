#ifndef filter_hh_INCLUDED
#define filter_hh_INCLUDED

#include <functional>

namespace Kakoune
{

class DisplayBuffer;

typedef std::function<void (DisplayBuffer& display_buffer)> FilterFunc;
typedef std::pair<std::string, FilterFunc> FilterAndId;

}

#endif // filter_hh_INCLUDED
