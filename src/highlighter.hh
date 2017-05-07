#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include "coord.hh"
#include "completion.hh"
#include "display_buffer.hh"
#include "exception.hh"
#include "flags.hh"
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
    Wrap = 1 << 0,
    Move = 1 << 1,
    Colorize = 1 << 2,

    All = Wrap | Move | Colorize,
};
constexpr bool with_bit_ops(Meta::Type<HighlightPass>) { return true; }

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
    // Offset of line and columns that must remain visible around cursor
    const DisplayCoord scroll_offset;
};

struct Highlighter
{
    Highlighter(HighlightPass passes) : m_passes{passes} {}
    virtual ~Highlighter() = default;

    void highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range)
    {
        if (pass & m_passes)
            do_highlight(context, pass, display_buffer, range);
    }

    void compute_display_setup(const Context& context, HighlightPass pass, DisplaySetup& setup)
    {
        if (pass & m_passes)
            do_compute_display_setup(context, pass, setup);
    }

    virtual bool has_children() const { return false; }
    virtual Highlighter& get_child(StringView path) { throw runtime_error("this highlighter do not hold children"); }
    virtual void add_child(HighlighterAndId&& hl) { throw runtime_error("this highlighter do not hold children"); }
    virtual void remove_child(StringView id) { throw runtime_error("this highlighter do not hold children"); }
    virtual Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const { throw runtime_error("this highlighter do not hold children"); }

    HighlightPass passes() const { return m_passes; }

private:
    virtual void do_highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) = 0;
    virtual void do_compute_display_setup(const Context& context, HighlightPass pass, DisplaySetup& setup) {}

    const HighlightPass m_passes;
};

template<typename Func>
struct SimpleHighlighter : public Highlighter
{
    SimpleHighlighter(Func func, HighlightPass pass)
      : Highlighter{pass}, m_func{std::move(func)} {}

private:
    void do_highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) override
    {
        m_func(context, pass, display_buffer, range);
    }
    Func m_func;
};

template<typename T>
std::unique_ptr<SimpleHighlighter<T>> make_simple_highlighter(T func, HighlightPass pass = HighlightPass::Colorize)
{
    return make_unique<SimpleHighlighter<T>>(std::move(func), pass);
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
