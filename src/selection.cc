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
    if (m_first <= m_last)
        m_first = std::min(m_first, selection.m_first);
    else
        m_first = std::max(m_first, selection.m_first);
    m_last = selection.m_last;
}

void Selection::register_with_buffer()
{
    Buffer& buffer = const_cast<Buffer&>(m_first.buffer());
    buffer.add_iterator_to_update(m_first);
    buffer.add_iterator_to_update(m_last);
}

void Selection::unregister_with_buffer()
{
    Buffer& buffer = const_cast<Buffer&>(m_first.buffer());
    buffer.remove_iterator_from_update(m_first);
    buffer.remove_iterator_from_update(m_last);
}

}
