#include "selection.hh"

namespace Kakoune
{

Selection::Selection(const BufferIterator& first, const BufferIterator& last)
    : m_first(first), m_last(last)
{
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
    return std::max(m_first, m_last) + 1;
}

void Selection::merge_with(const Selection& selection)
{
    if (m_first < m_last)
        m_first = std::min(m_first, selection.m_first);
    if (m_first > m_last)
        m_first = std::max(m_first, selection.m_first);
    m_last = selection.m_last;
}

void Selection::avoid_eol()
{
    m_first.clamp(true);
    m_last.clamp(true);
}

void Selection::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    m_first.on_insert(begin.coord(), end.coord());
    m_last.on_insert(begin.coord(), end.coord());
}

void Selection::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    m_first.on_erase(begin.coord(), end.coord());
    m_last.on_erase(begin.coord(), end.coord());
}

void Selection::register_with_buffer()
{
    Buffer& buffer = const_cast<Buffer&>(m_first.buffer());
    buffer.add_change_listener(*this);
}

void Selection::unregister_with_buffer()
{
    Buffer& buffer = const_cast<Buffer&>(m_first.buffer());
    buffer.remove_change_listener(*this);
}

}
