#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include "function_group.hh"
#include "function_registry.hh"
#include "memoryview.hh"
#include "string.hh"
#include "utils.hh"

#include <functional>

namespace Kakoune
{

class DisplayBuffer;
class Window;

// An Highlighter is a function which mutates a DisplayBuffer in order to
// change the visual representation of a file. It could be changing text
// color, adding information text (line numbering for example) or replacing
// buffer content (folding for example)

typedef std::function<void (const Window& window, DisplayBuffer& display_buffer)> HighlighterFunc;
typedef std::pair<String, HighlighterFunc> HighlighterAndId;
typedef memoryview<String> HighlighterParameters;

using HighlighterFactory = std::function<HighlighterAndId (HighlighterParameters params)>;

using HighlighterGroup = FunctionGroup<const Window&, DisplayBuffer&>;

struct HighlighterRegistry : FunctionRegistry<HighlighterFactory>,
                             Singleton<HighlighterRegistry>
{};

}

#endif // highlighter_hh_INCLUDED
