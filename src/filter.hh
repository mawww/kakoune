#ifndef filter_hh_INCLUDED
#define filter_hh_INCLUDED

#include <string>
#include <functional>

namespace Kakoune
{

class Buffer;
class Modification;

// A Filter is a function which is applied to a Buffer and a pending
// Modification in order to mutate the Buffer or the Modification
// prior to it's application.

typedef std::function<void (Buffer& buffer, Modification& modification)> FilterFunc;
typedef std::pair<std::string, FilterFunc> FilterAndId;

}

#endif // filter_hh_INCLUDED
