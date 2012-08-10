#ifndef filter_hh_INCLUDED
#define filter_hh_INCLUDED

#include "string.hh"
#include <functional>

namespace Kakoune
{

class Buffer;
class BufferIterator;

// A Filter is a function which is applied to a Buffer and a pending
// Modification in order to mutate the Buffer or the Modification
// prior to it's application.

typedef std::function<void (Buffer& buffer, BufferIterator& position, String& content)> FilterFunc;
typedef std::pair<String, FilterFunc> FilterAndId;

}

#endif // filter_hh_INCLUDED
