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
         typename CodepointType = Codepoint,
         typename DifferenceType = CharCount,
         typename InvalidPolicy = utf8::InvalidPolicy::Pass>
class iterator : public std::iterator<std::bidirectional_iterator_tag,
                                      CodepointType, DifferenceType>
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
        utf8::to_next(m_it, m_end);
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
        utf8::to_previous(m_it, m_begin);
        invalidate_value();
        return *this;
    }

    iterator operator--(int)
    {
        iterator save = *this;
        --*this;
        return save;
    }

    iterator operator+(DifferenceType count) const
    {
        if (count < 0)
            return operator-(-count);

        iterator res = *this;
        while (count--)
            ++res;
        return res;
    }

    iterator operator-(DifferenceType count) const
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

    DifferenceType operator-(const iterator& other) const
    {
        return (DifferenceType)utf8::distance(other.m_it, m_it);
    }

    CodepointType operator*() const
    {
        return get_value();
    }

    const Iterator& base() const { return m_it; }
    Iterator& base() { return m_it; }

private:
    void invalidate_value() { m_value = -1; }
    CodepointType get_value() const
    {
        if (m_value == (CodepointType)-1)
            m_value = (CodepointType)utf8::codepoint<InvalidPolicy>(m_it, m_end);
        return m_value;
    }

    Iterator m_it;
    Iterator m_begin;
    Iterator m_end;
    mutable CodepointType m_value = -1;
};

}

}
#endif // utf8_iterator_hh_INCLUDED
