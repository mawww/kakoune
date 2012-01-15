#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include <string>
#include <functional>
#include <vector>

namespace Kakoune
{

class DisplayBuffer;

// An Highlighter is a function which mutates a DisplayBuffer in order to
// change the visual representation of a file. It could be changing text
// color, adding information text (line numbering for example) or replacing
// buffer content (folding for example)

typedef std::function<void (DisplayBuffer& display_buffer)> HighlighterFunc;
typedef std::pair<std::string, HighlighterFunc> HighlighterAndId;
typedef std::vector<std::string> HighlighterParameters;


}

#endif // highlighter_hh_INCLUDED
