#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include "coord.hh"
#include "completion.hh"
#include "display_buffer.hh"
#include "exception.hh"
#include "hash_map.hh"
#include "array_view.hh"
#include "string.hh"
#include "utils.hh"

#include <functional>

namespace Kakoune
{

class Context;

enum class HighlightPass
{
    Wrap,
    Move,
    Colorize,
};

// An Highlighter is a function which mutates a DisplayBuffer in order to
// change the visual representation of a file. It could be changing text
// color, adding information text (line numbering for example) or replacing
// buffer content (folding for example)

struct Highlighter;

using HighlighterAndId = std::pair<String, std::unique_ptr<Highlighter>>;

struct DisplaySetup
{
    // Window position relative to the buffer origin
    DisplayCoord window_pos;
    // Range of lines and columns from the buffer that will get displayed
    DisplayCoord window_range;

    // Position of the cursor in the window
    DisplayCoord cursor_pos;
};

struct Highlighter
{
    virtual ~Highlighter() = default;
    virtual void highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) = 0;

    virtual void compute_display_setup(const Context& context, HighlightPass pass,
                                       DisplayCoord scroll_offset, DisplaySetup& setup) {}

    virtual bool has_children() const { return false; }
    virtual Highlighter& get_child(StringView path) { throw runtime_error("this highlighter do not hold children"); }
    virtual void add_child(HighlighterAndId&& hl) { throw runtime_error("this highlighter do not hold children"); }
    virtual void remove_child(StringView id) { throw runtime_error("this highlighter do not hold children"); }
    virtual Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const { throw runtime_error("this highlighter do not hold children"); }
};

template<typename Func>
struct SimpleHighlighter : public Highlighter
{
    SimpleHighlighter(Func func) : m_func(std::move(func)) {}
    void highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) override
    {
        m_func(context, pass, display_buffer, range);
    }
private:
    Func m_func;
};

template<typename T>
std::unique_ptr<SimpleHighlighter<T>> make_simple_highlighter(T func)
{
    return make_unique<SimpleHighlighter<T>>(std::move(func));
}

using HighlighterParameters = ConstArrayView<String>;
using HighlighterFactory = std::function<HighlighterAndId (HighlighterParameters params)>;

struct HighlighterFactoryAndDocstring
{
    HighlighterFactory factory;
    String docstring;
};

struct HighlighterRegistry : HashMap<String, HighlighterFactoryAndDocstring, MemoryDomain::Highlight>,
                             Singleton<HighlighterRegistry>
{};

}

#endif // highlighter_hh_INCLUDED
