#include "selection.hh"

#include "utf8.hh"

namespace Kakoune
{

void Range::merge_with(const Range& range)
{
    if (m_first < m_last)
        m_first = std::min(m_first, range.m_first);
    if (m_first > m_last)
        m_first = std::max(m_first, range.m_first);
    m_last = range.m_last;
}

BufferIterator Range::begin() const
{
    return std::min(m_first, m_last);
}

BufferIterator Range::end() const
{
    return utf8::next(std::max(m_first, m_last));
}

void Range::check_invariant() const
{
#ifdef KAK_DEBUG
    assert(m_first.is_valid());
    assert(m_last.is_valid());
    assert(utf8::is_character_start(m_first));
    assert(utf8::is_character_start(m_last));
#endif
}

static void avoid_eol(BufferIterator& it)
{
    const auto column = it.column();
    if (column != 0 and column == it.buffer().line_length(it.line()) - 1)
        it = utf8::previous(it);
}

void Selection::avoid_eol()
{
    Kakoune::avoid_eol(first());
    Kakoune::avoid_eol(last());
}

}
