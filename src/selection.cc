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

void Selection::on_modification(const Modification& modification)
{
    m_first.update(modification);
    m_last.update(modification);
}

void Selection::register_with_buffer()
{
    const_cast<Buffer&>(m_first.buffer()).register_modification_listener(this);
}

void Selection::unregister_with_buffer()
{
    const_cast<Buffer&>(m_first.buffer()).unregister_modification_listener(this);
}

}
