#ifndef utf8_iterator_hh_INCLUDED
#define utf8_iterator_hh_INCLUDED

#include "utf8.hh"

#include <iterator>

namespace Kakoune
{

namespace utf8
{

// adapter for an iterator on bytes which permits to iterate
// on unicode codepoints instead.
template<typename Iterator,
         typename InvalidPolicy = utf8::InvalidPolicy::Pass>
class iterator : public std::iterator<std::bidirectional_iterator_tag,
                                      Codepoint, CharCount>
{
public:
    iterator() = default;

    iterator(Iterator it, Iterator begin, Iterator end)
        : m_it{std::move(it)}, m_begin{std::move(begin)}, m_end{std::move(end)}
    {}

    template<typename Container>
    iterator(Iterator it, const Container& c)
        : m_it{std::move(it)}, m_begin{std::begin(c)}, m_end{std::end(c)}
    {}

    iterator& operator++()
    {
        m_it = utf8::next(m_it, m_end);
        invalidate_value();
        return *this;
    }

    iterator operator++(int)
    {
        iterator save = *this;
        ++*this;
        return save;
    }

    iterator& operator--()
    {
        m_it = utf8::previous(m_it, m_begin);
        invalidate_value();
        return *this;
    }

    iterator operator--(int)
    {
        iterator save = *this;
        --*this;
        return save;
    }

    iterator operator+(CharCount count) const
    {
        if (count < 0)
            return operator-(-count);

        iterator res = *this;
        while (count--)
            ++res;
        return res;
    }

    iterator operator-(CharCount count) const
    {
        if (count < 0)
            return operator+(-count);

        iterator res = *this;
        while (count--)
            --res;
        return res;
    }

    bool operator==(const iterator& other) const { return m_it == other.m_it; }
    bool operator!=(const iterator& other) const { return m_it != other.m_it; }

    bool operator< (const iterator& other) const { return m_it < other.m_it; }
    bool operator<= (const iterator& other) const { return m_it <= other.m_it; }

    bool operator> (const iterator& other) const { return m_it > other.m_it; }
    bool operator>= (const iterator& other) const { return m_it >= other.m_it; }

    bool operator==(const Iterator& other) { return m_it == other; }
    bool operator!=(const Iterator& other) { return m_it != other; }

    bool operator< (const Iterator& other) const { return m_it < other; }
    bool operator<= (const Iterator& other) const { return m_it <= other; }

    bool operator> (const Iterator& other) const { return m_it > other; }
    bool operator>= (const Iterator& other) const { return m_it >= other; }

    CharCount operator-(const iterator& other) const
    {
        return utf8::distance(other.m_it, m_it);
    }

    Codepoint operator*() const
    {
        return get_value();
    }

    const Iterator& base() const { return m_it; }
    Iterator& base() { return m_it; }

private:
    void invalidate_value() { m_value = -1; }
    Codepoint get_value() const
    {
        if (m_value == -1)
            m_value = utf8::codepoint<InvalidPolicy>(m_it, m_end);
        return m_value;
    }

    Iterator m_it;
    Iterator m_begin;
    Iterator m_end;
    mutable Codepoint m_value = -1;
};

}

}
#endif // utf8_iterator_hh_INCLUDED
