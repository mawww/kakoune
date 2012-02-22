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

static void update_iterator(const Modification& modification,
                            BufferIterator& iterator)
{
    if (iterator < modification.position)
        return;

    size_t length = modification.content.length();
    if (modification.type == Modification::Erase)
    {
        // do not move length on the other side of the inequality,
        // as modification.position + length may be after buffer end
        if (iterator - length <= modification.position)
            iterator = modification.position;
        else
            iterator -= length;

        if (iterator.is_end())
            --iterator;
    }
    else
    {
        assert(modification.type == Modification::Insert);
        iterator += length;
    }
}

void Selection::on_modification(const Modification& modification)
{
    update_iterator(modification, m_first);
    update_iterator(modification, m_last);
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
