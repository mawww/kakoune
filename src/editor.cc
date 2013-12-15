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
      m_edition_level(0),
      m_selections(buffer, {BufferCoord{}})
{}

void Editor::erase()
{
    scoped_edition edition(*this);
    for (auto& sel : m_selections)
    {
        Kakoune::erase(*m_buffer, sel);
        avoid_eol(*m_buffer, sel);
    }
}

static BufferIterator prepare_insert(Buffer& buffer, const Selection& sel,
                                     InsertMode mode)
{
    switch (mode)
    {
    case InsertMode::Insert:
        return buffer.iterator_at(sel.min());
    case InsertMode::Replace:
        return Kakoune::erase(buffer, sel);
    case InsertMode::Append:
    {
        // special case for end of lines, append to current line instead
        auto pos = buffer.iterator_at(sel.max());
        return *pos == '\n' ? pos : utf8::next(pos);
    }
    case InsertMode::InsertAtLineBegin:
        return buffer.iterator_at(sel.min().line);
    case InsertMode::AppendAtLineEnd:
        return buffer.iterator_at({sel.max().line, buffer[sel.max().line].length() - 1});
    case InsertMode::InsertAtNextLineBegin:
        return buffer.iterator_at(sel.max().line+1);
    case InsertMode::OpenLineBelow:
        return buffer.insert(buffer.iterator_at(sel.max().line + 1), "\n");
    case InsertMode::OpenLineAbove:
        return buffer.insert(buffer.iterator_at(sel.min().line), "\n");
    }
    kak_assert(false);
    return {};
}

void Editor::insert(const String& str, InsertMode mode)
{
    scoped_edition edition(*this);

    for (auto& sel : m_selections)
    {
        auto pos = prepare_insert(*m_buffer, sel, mode);
        pos = m_buffer->insert(pos, str);
        if (mode == InsertMode::Replace and pos != m_buffer->end())
        {
            sel.first() = pos.coord();
            sel.last()  = str.empty() ?
                 pos.coord() : (pos + str.byte_count_to(str.char_length() - 1)).coord();
        }
        avoid_eol(*m_buffer, sel);
    }
    check_invariant();
}

void Editor::insert(memoryview<String> strings, InsertMode mode)
{
    scoped_edition edition(*this);
    if (strings.empty())
        return;

    for (size_t i = 0; i < selections().size(); ++i)
    {
        auto& sel = m_selections[i];
        auto pos = prepare_insert(*m_buffer, sel, mode);
        const String& str = strings[std::min(i, strings.size()-1)];
        pos = m_buffer->insert(pos, str);
        if (mode == InsertMode::Replace and pos != m_buffer->end())
        {
            sel.first() = pos.coord();
            sel.last()  = (str.empty() ?
                           pos : pos + str.byte_count_to(str.char_length() - 1)).coord();
        }
        avoid_eol(*m_buffer, sel);
    }
    check_invariant();
}

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

void Editor::begin_edition()
{
    ++m_edition_level;
}

void Editor::end_edition()
{
    kak_assert(m_edition_level > 0);
    if (m_edition_level == 1)
        m_buffer->commit_undo_group();

    --m_edition_level;
}

}
