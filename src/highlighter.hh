#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include <functional>

#include "string.hh"
#include "utils.hh"
#include "memoryview.hh"
#include "function_registry.hh"

namespace Kakoune
{

class DisplayBuffer;
class Window;

// An Highlighter is a function which mutates a DisplayBuffer in order to
// change the visual representation of a file. It could be changing text
// color, adding information text (line numbering for example) or replacing
// buffer content (folding for example)

typedef std::function<void (DisplayBuffer& display_buffer)> HighlighterFunc;
typedef std::pair<String, HighlighterFunc> HighlighterAndId;
typedef memoryview<String> HighlighterParameters;

using HighlighterFactory = std::function<HighlighterAndId (const HighlighterParameters& params)>;

struct HighlighterRegistry : FunctionRegistry<HighlighterFactory>,
                             Singleton<HighlighterRegistry>
{};

}

#endif // highlighter_hh_INCLUDED
