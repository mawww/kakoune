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
template<typename BaseIt, typename Sentinel = BaseIt,
         typename CodepointType  = Codepoint,
         typename DifferenceType = CharCount,
         typename InvalidPolicy  = utf8::InvalidPolicy::Pass>
class iterator
    : public std::iterator<std::bidirectional_iterator_tag, CodepointType,
                           DifferenceType, CodepointType*, CodepointType>
{
public:
    iterator()                            = default;
    constexpr static bool noexcept_policy = noexcept(InvalidPolicy{}(0));

    iterator(BaseIt it, Sentinel begin, Sentinel end) noexcept
        : m_it{std::move(it)}, m_begin{std::move(begin)}, m_end{std::move(end)}
    {}

    template<typename Container>
    iterator(BaseIt it, const Container& c) noexcept
        : m_it{std::move(it)}, m_begin{std::begin(c)}, m_end{std::end(c)}
    {}

    iterator& operator++() noexcept
    {
        utf8::to_next(m_it, m_end);
        return *this;
    }

    iterator operator++(int) noexcept
    {
        iterator save = *this;
        ++*this;
        return save;
    }

    iterator& operator--() noexcept
    {
        utf8::to_previous(m_it, m_begin);
        return *this;
    }

    iterator operator--(int) noexcept
    {
        iterator save = *this;
        --*this;
        return save;
    }

    iterator operator+(DifferenceType count) const noexcept
    {
        iterator res = *this;
        res += count;
        return res;
    }

    iterator& operator+=(DifferenceType count) noexcept
    {
        if (count < 0)
            return operator-=(-count);

        while (count--)
            operator++();
        return *this;
    }

    iterator operator-(DifferenceType count) const noexcept
    {
        iterator res = *this;
        res -= count;
        return res;
    }

    iterator& operator-=(DifferenceType count) noexcept
    {
        if (count < 0)
            return operator+=(-count);

        while (count--)
            operator--();
        return *this;
    }

    bool operator==(const iterator& other) const noexcept
    {
        return m_it == other.m_it;
    }
    bool operator!=(const iterator& other) const noexcept
    {
        return m_it != other.m_it;
    }

    bool operator<(const iterator& other) const noexcept
    {
        return m_it < other.m_it;
    }
    bool operator<=(const iterator& other) const noexcept
    {
        return m_it <= other.m_it;
    }

    bool operator>(const iterator& other) const noexcept
    {
        return m_it > other.m_it;
    }
    bool operator>=(const iterator& other) const noexcept
    {
        return m_it >= other.m_it;
    }

    template<typename T>
    std::enable_if_t<std::is_same<T, BaseIt>::value
                         or std::is_same<T, Sentinel>::value,
                     bool>
    operator==(const T& other) const noexcept
    {
        return m_it == other;
    }

    template<typename T>
    std::enable_if_t<std::is_same<T, BaseIt>::value
                         or std::is_same<T, Sentinel>::value,
                     bool>
    operator!=(const T& other) const noexcept
    {
        return m_it != other;
    }

    bool operator<(const BaseIt& other) const noexcept { return m_it < other; }
    bool operator<=(const BaseIt& other) const noexcept
    {
        return m_it <= other;
    }

    bool operator>(const BaseIt& other) const noexcept { return m_it > other; }
    bool operator>=(const BaseIt& other) const noexcept
    {
        return m_it >= other;
    }

    DifferenceType operator-(const iterator& other) const
        noexcept(noexcept_policy)
    {
        return (DifferenceType)utf8::distance<InvalidPolicy>(other.m_it, m_it);
    }

    CodepointType operator*() const noexcept(noexcept_policy)
    {
        return (CodepointType)utf8::codepoint<InvalidPolicy>(m_it, m_end);
    }

    CodepointType read() noexcept(noexcept_policy)
    {
        return (CodepointType)utf8::read_codepoint<InvalidPolicy>(m_it, m_end);
    }

    const BaseIt& base() const noexcept { return m_it; }

private:
    BaseIt m_it;
    Sentinel m_begin;
    Sentinel m_end;
};

}

}
#endif // utf8_iterator_hh_INCLUDED
