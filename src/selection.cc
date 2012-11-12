#include "selection.hh"

#include "utf8.hh"

namespace Kakoune
{

Selection::Selection(const BufferIterator& first, const BufferIterator& last)
    : m_first(first), m_last(last)
{
    check_invariant();
    register_with_buffer();
}

Selection::Selection(const Selection& other)
    : m_first(other.m_first), m_last(other.m_last)
{
   register_with_buffer();
}

Selection::~Selection()
{
   unregister_with_buffer();
}

Selection& Selection::operator=(const Selection& other)
{
   const bool new_buffer = &m_first.buffer() != &other.m_first.buffer();
   if (new_buffer)
       unregister_with_buffer();

   m_first    = other.m_first;
   m_last     = other.m_last;

   if (new_buffer)
       register_with_buffer();

   return *this;
}

BufferIterator Selection::begin() const
{
    return std::min(m_first, m_last);
}

BufferIterator Selection::end() const
{
    return utf8::next(std::max(m_first, m_last));
}

void Selection::merge_with(const Selection& selection)
{
    if (m_first < m_last)
        m_first = std::min(m_first, selection.m_first);
    if (m_first > m_last)
        m_first = std::max(m_first, selection.m_first);
    m_last = selection.m_last;
}

static void avoid_eol(BufferIterator& it)
{
    const auto column = it.column();
    if (column != 0 and column == it.buffer().line_length(it.line()) - 1)
        it = utf8::previous(it);
}

void Selection::avoid_eol()
{
    Kakoune::avoid_eol(m_first);
    Kakoune::avoid_eol(m_last);
}

void Selection::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    m_first.on_insert(begin.coord(), end.coord());
    m_last.on_insert(begin.coord(), end.coord());
    check_invariant();
}

void Selection::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    m_first.on_erase(begin.coord(), end.coord());
    m_last.on_erase(begin.coord(), end.coord());
    check_invariant();
}

void Selection::register_with_buffer()
{
    m_first.buffer().add_change_listener(*this);
}

void Selection::unregister_with_buffer()
{
    m_first.buffer().remove_change_listener(*this);
}

void Selection::check_invariant() const
{
    assert(utf8::is_character_start(m_first));
    assert(utf8::is_character_start(m_last));
}

}
