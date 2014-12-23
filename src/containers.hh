#ifndef containers_hh_INCLUDED
#define containers_hh_INCLUDED

#include <algorithm>
#include <utility>
#include <iterator>

namespace Kakoune
{

template<typename Container>
struct ReversedContainer
{
    using iterator = decltype(std::declval<Container>().rbegin());
    ReversedContainer(Container& container) : m_container(container) {}

    iterator begin() { return m_container.rbegin(); }
    iterator end()   { return m_container.rend(); }

private:
    Container& m_container;
};

template<typename Container>
ReversedContainer<Container> reversed(Container&& container)
{
    return ReversedContainer<Container>(container);
}

template<typename Iterator, typename Filter>
struct FilteredIterator : std::iterator<std::forward_iterator_tag,
                                        typename Iterator::value_type>
{
    FilteredIterator(Filter filter, Iterator it, Iterator end)
        : m_it(std::move(it)), m_end(std::move(end)), m_filter(std::move(filter))
    {
        do_filter();
    }

    auto operator*() -> decltype(*std::declval<Iterator>()) { return *m_it; }
    FilteredIterator& operator++() { ++m_it; do_filter(); return *this; }
    FilteredIterator operator++(int) { auto copy = *this; ++(*this); return copy; }

    friend bool operator==(const FilteredIterator& lhs, const FilteredIterator& rhs)
    {
        return lhs.m_it == rhs.m_it;
    }

    friend bool operator!=(const FilteredIterator& lhs, const FilteredIterator& rhs)
    {
        return not (lhs == rhs);
    }

    Iterator base() const { return m_it; }

private:
    void do_filter()
    {
        while (m_it != m_end and not m_filter(*m_it))
            ++m_it;
    }

    Iterator m_it;
    Iterator m_end;
    Filter   m_filter;
};

template<typename Container, typename Filter>
struct FilteredContainer
{
    using iterator = FilteredIterator<decltype(begin(std::declval<Container>())), Filter>;
    FilteredContainer(Container& container, Filter filter)
        : m_container(container), m_filter(std::move(filter)) {}

    iterator begin() const { return iterator(m_filter, m_container.begin(), m_container.end()); }
    iterator end()   const { return iterator(m_filter, m_container.end(), m_container.end()); }

private:
    Container& m_container;
    Filter m_filter;
};

template<typename Container, typename Filter>
FilteredContainer<Container, Filter> filtered(Container&& container, Filter filter)
{
    return FilteredContainer<Container, Filter>(container, std::move(filter));
}

template<typename Iterator, typename Transform>
struct TransformedIterator : std::iterator<std::forward_iterator_tag,
                                           typename std::remove_reference<decltype(std::declval<Transform>()(*std::declval<Iterator>()))>::type>
{
    TransformedIterator(Transform transform, Iterator it)
        : m_it(std::move(it)), m_transform(std::move(transform)) {}

    auto operator*() -> decltype(std::declval<Transform>()(*std::declval<Iterator>())) { return m_transform(*m_it); }
    TransformedIterator& operator++() { ++m_it; return *this; }
    TransformedIterator operator++(int) { auto copy = *this; ++m_it; return copy; }

    friend bool operator==(const TransformedIterator& lhs, const TransformedIterator& rhs)
    {
        return lhs.m_it == rhs.m_it;
    }

    friend bool operator!=(const TransformedIterator& lhs, const TransformedIterator& rhs)
    {
        return not (lhs == rhs);
    }

    Iterator base() const { return m_it; }

private:
    Iterator m_it;
    Transform m_transform;
};


template<typename Container, typename Transform>
struct TransformedContainer
{
    using iterator = TransformedIterator<decltype(begin(std::declval<Container>())), Transform>;
    TransformedContainer(Container& container, Transform transform)
        : m_container(container), m_transform(std::move(transform)) {}

    iterator begin() const { return iterator(m_transform, m_container.begin()); }
    iterator end()   const { return iterator(m_transform, m_container.end()); }

private:
    Container& m_container;
    Transform m_transform;
};

template<typename Container, typename Transform>
TransformedContainer<Container, Transform> transformed(Container&& container, Transform transform)
{
    return TransformedContainer<Container, Transform>(container, std::move(transform));
}

// Todo: move that into the following functions once we can remove the decltype
//       return type.
using std::begin;
using std::end;

template<typename Container, typename T>
auto find(Container&& container, const T& value) -> decltype(begin(container))
{
    return std::find(begin(container), end(container), value);
}

template<typename Container, typename T>
auto find_if(Container&& container, T op) -> decltype(begin(container))
{
    return std::find_if(begin(container), end(container), op);
}

template<typename Container, typename T>
bool contains(Container&& container, const T& value)
{
    return find(container, value) != end(container);
}

template<typename Container, typename U>
void unordered_erase(Container&& vec, U&& value)
{
    auto it = find(vec, std::forward<U>(value));
    if (it != vec.end())
    {
        using std::swap;
        swap(vec.back(), *it);
        vec.pop_back();
    }
}

}

#endif // containers_hh_INCLUDED
