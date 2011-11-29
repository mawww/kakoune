#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include <functional>

namespace Kakoune
{

class DisplayBuffer;

typedef std::function<void (DisplayBuffer& display_buffer)> HighlighterFunc;
typedef std::pair<std::string, HighlighterFunc> HighlighterAndId;

}

#endif // highlighter_hh_INCLUDED
