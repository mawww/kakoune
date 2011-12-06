#ifndef filter_hh_INCLUDED
#define filter_hh_INCLUDED

#include <string>
#include <functional>

namespace Kakoune
{

class Buffer;
class Modification;

typedef std::function<void (Buffer& buffer, Modification& modification)> FilterFunc;
typedef std::pair<std::string, FilterFunc> FilterAndId;

}

#endif // filter_hh_INCLUDED
