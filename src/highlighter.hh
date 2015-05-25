#ifndef highlighter_hh_INCLUDED
#define highlighter_hh_INCLUDED

#include "coord.hh"
#include "completion.hh"
#include "display_buffer.hh"
#include "exception.hh"
#include "id_map.hh"
#include "array_view.hh"
#include "string.hh"
#include "utils.hh"

#include <functional>

namespace Kakoune
{

class Context;

enum class HighlightFlags
{
    Highlight,
    MoveOnly
};

// An Highlighter is a function which mutates a DisplayBuffer in order to
// change the visual representation of a file. It could be changing text
// color, adding information text (line numbering for example) or replacing
// buffer content (folding for example)

struct Highlighter;

using HighlighterAndId = std::pair<String, std::unique_ptr<Highlighter>>;

struct BufferRange;

struct Highlighter
{
    virtual ~Highlighter() {}
    virtual void highlight(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer, BufferRange range) = 0;

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
    virtual void highlight(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer, BufferRange range) override
    {
        m_func(context, flags, display_buffer, range);
    }
private:
    Func m_func;
};

template<typename T>
std::unique_ptr<SimpleHighlighter<T>> make_simple_highlighter(T func)
{
    return std::make_unique<SimpleHighlighter<T>>(std::move(func));
}

using HighlighterParameters = ConstArrayView<String>;
using HighlighterFactory = std::function<HighlighterAndId (HighlighterParameters params)>;

struct HighlighterFactoryAndDocstring
{
    HighlighterFactory factory;
    String docstring;
};

struct HighlighterRegistry : IdMap<HighlighterFactoryAndDocstring>,
                             Singleton<HighlighterRegistry>
{};

}

#endif // highlighter_hh_INCLUDED
