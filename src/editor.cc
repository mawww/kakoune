#include "editor.hh"

#include "exception.hh"
#include "register.hh"
#include "register_manager.hh"
#include "utf8_iterator.hh"
#include "utils.hh"

#include <array>

namespace Kakoune
{

Editor::Editor(Buffer& buffer)
    : m_buffer(&buffer),
      m_selections(buffer, {BufferCoord{}})
{}

std::vector<String> Editor::selections_content() const
{
    std::vector<String> contents;
    for (auto& sel : m_selections)
        contents.push_back(m_buffer->string(sel.min(), m_buffer->char_next(sel.max())));
    return contents;
}

class ModifiedRangesListener : public BufferChangeListener_AutoRegister
{
public:
    ModifiedRangesListener(Buffer& buffer)
        : BufferChangeListener_AutoRegister(buffer) {}

    void on_insert(const Buffer& buffer, BufferCoord begin, BufferCoord end)
    {
        m_ranges.update_insert(buffer, begin, end);
        auto it = std::upper_bound(m_ranges.begin(), m_ranges.end(), begin,
                                   [](BufferCoord c, const Selection& sel)
                                   { return c < sel.min(); });
        m_ranges.emplace(it, begin, buffer.char_prev(end));
    }

    void on_erase(const Buffer& buffer, BufferCoord begin, BufferCoord end)
    {
        m_ranges.update_erase(buffer, begin, end);
        auto pos = std::min(begin, buffer.back_coord());
        auto it = std::upper_bound(m_ranges.begin(), m_ranges.end(), pos,
                                   [](BufferCoord c, const Selection& sel)
                                   { return c < sel.min(); });
        m_ranges.emplace(it, pos, pos);
    }
    SelectionList& ranges() { return m_ranges; }

private:
    SelectionList m_ranges;
};

inline bool touches(const Buffer& buffer, const Range& lhs, const Range& rhs)
{
    return lhs.min() <= rhs.min() ? buffer.char_next(lhs.max()) >= rhs.min()
                                  : lhs.min() <= buffer.char_next(rhs.max());
}

bool Editor::undo()
{
    using namespace std::placeholders;
    ModifiedRangesListener listener(buffer());
    bool res = m_buffer->undo();
    if (res and not listener.ranges().empty())
    {
        m_selections = std::move(listener.ranges());
        m_selections.set_main_index(m_selections.size() - 1);
        m_selections.merge_overlapping(std::bind(touches, std::ref(buffer()), _1, _2));
    }
    check_invariant();
    return res;
}

bool Editor::redo()
{
    using namespace std::placeholders;
    ModifiedRangesListener listener(buffer());
    bool res = m_buffer->redo();
    if (res and not listener.ranges().empty())
    {
        m_selections = std::move(listener.ranges());
        m_selections.set_main_index(m_selections.size() - 1);
        m_selections.merge_overlapping(std::bind(touches, std::ref(buffer()), _1, _2));
    }
    check_invariant();
    return res;
}

void Editor::check_invariant() const
{
#ifdef KAK_DEBUG
    kak_assert(not m_selections.empty());
    m_selections.check_invariant();
    buffer().check_invariant();
#endif
}

}
