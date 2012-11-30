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

Selection::Selection(const BufferIterator& first, const BufferIterator& last,
                     CaptureList captures)
    : Range{first, last}, m_captures{std::move(captures)}
{
    check_invariant();
    register_with_buffer();
}

Selection::Selection(const Selection& other)
    : Range(other), m_captures(other.m_captures)
{
   register_with_buffer();
}

Selection::Selection(Selection&& other)
    : Range{other},
      m_captures{std::move(other.m_captures)}
{
   register_with_buffer();
}

Selection::~Selection()
{
   unregister_with_buffer();
}

Selection& Selection::operator=(const Selection& other)
{
   const bool new_buffer = &first().buffer() != &other.first().buffer();
   if (new_buffer)
       unregister_with_buffer();

   first() = other.first();
   last()  = other.last();
   m_captures = other.m_captures;

   if (new_buffer)
       register_with_buffer();

   return *this;
}

Selection& Selection::operator=(Selection&& other)
{
   const bool new_buffer = &first().buffer() != &other.first().buffer();
   if (new_buffer)
       unregister_with_buffer();

   first() = other.first();
   last()  = other.last();
   m_captures = std::move(other.m_captures);

   if (new_buffer)
       register_with_buffer();

   return *this;
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

void Selection::on_insert(const BufferIterator& begin, const BufferIterator& end)
{
    first().on_insert(begin.coord(), end.coord());
    last().on_insert(begin.coord(), end.coord());
    check_invariant();
}

void Selection::on_erase(const BufferIterator& begin, const BufferIterator& end)
{
    first().on_erase(begin.coord(), end.coord());
    last().on_erase(begin.coord(), end.coord());
    check_invariant();
}

void Selection::register_with_buffer()
{
    first().buffer().add_change_listener(*this);
}

void Selection::unregister_with_buffer()
{
    first().buffer().remove_change_listener(*this);
}

void Selection::check_invariant() const
{
    assert(utf8::is_character_start(first()));
    assert(utf8::is_character_start(last()));
}

}
