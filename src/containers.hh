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
        : m_it{std::move(it)}, m_end{std::move(end)}, m_filter{std::move(filter)}
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
        : m_container{container}, m_filter{std::move(filter)} {}

    iterator begin() const { return {m_filter, m_container.begin(), m_container.end()}; }
    iterator end()   const { return {m_filter, m_container.end(), m_container.end()}; }

private:
    Container& m_container;
    Filter m_filter;
};

template<typename Container, typename Filter>
FilteredContainer<Container, Filter> filtered(Container&& container, Filter filter)
{
    return {container, std::move(filter)};
}

template<typename I, typename T>
using TransformedResult = decltype(std::declval<T>()(*std::declval<I>()));

template<typename Iterator, typename Transform>
struct TransformedIterator : std::iterator<std::forward_iterator_tag,
                                           typename std::remove_reference<TransformedResult<Iterator, Transform>>::type>
{
    TransformedIterator(Transform transform, Iterator it)
        : m_it{std::move(it)}, m_transform{std::move(transform)} {}

    auto operator*() -> TransformedResult<Iterator, Transform> { return m_transform(*m_it); }
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
        : m_container{container}, m_transform{std::move(transform)} {}

    iterator begin() const { return {m_transform, m_container.begin()}; }
    iterator end()   const { return {m_transform, m_container.end()}; }

private:
    Container& m_container;
    Transform m_transform;
};

template<typename Container, typename Transform>
TransformedContainer<Container, Transform> transformed(Container&& container, Transform transform)
{
    return {container, std::move(transform)};
}

template<typename Iterator1, typename Iterator2, typename ValueType = typename Iterator1::value_type>
struct ConcatenatedIterator : std::iterator<std::forward_iterator_tag, ValueType>
{
    static_assert(std::is_convertible<typename Iterator1::value_type, ValueType>::value, "");
    static_assert(std::is_convertible<typename Iterator2::value_type, ValueType>::value, "");

    ConcatenatedIterator(Iterator1 it1, Iterator1 end1, Iterator2 it2)
        : m_it1(std::move(it1)), m_end1(std::move(end1)), m_it2(std::move(it2)) {}

    decltype(*std::declval<Iterator1>()) operator*() { return is2() ? *m_it2 : *m_it1; }
    ConcatenatedIterator& operator++() { if (is2()) ++m_it2; else ++m_it1; return *this; }
    ConcatenatedIterator operator++(int) { auto copy = *this; ++*this; return copy; }

    friend bool operator==(const ConcatenatedIterator& lhs, const ConcatenatedIterator& rhs)
    {
        return lhs.m_it1 == rhs.m_it1 and lhs.m_end1 == rhs.m_end1 and lhs.m_it2 == rhs.m_it2;
    }

    friend bool operator!=(const ConcatenatedIterator& lhs, const ConcatenatedIterator& rhs)
    {
        return not (lhs == rhs);
    }

private:
    bool is2() const { return m_it1 == m_end1; }

    Iterator1 m_it1;
    Iterator1 m_end1;
    Iterator2 m_it2;
};


template<typename Container1, typename Container2>
struct ConcatenatedContainer
{
    using iterator = ConcatenatedIterator<decltype(begin(std::declval<Container1>())),
                                          decltype(begin(std::declval<Container2>()))>;

    ConcatenatedContainer(Container1& container1, Container2& container2)
        : m_container1{container1}, m_container2{container2} {}

    iterator begin() const { return {m_container1.begin(), m_container1.end(), m_container2.begin()}; }
    iterator end()   const { return {m_container1.end(), m_container1.end(), m_container2.end()}; }

private:
    Container1& m_container1;
    Container2& m_container2;
};

template<typename Container1, typename Container2>
ConcatenatedContainer<Container1, Container2> concatenated(Container1&& container1, Container2&& container2)
{
    return {container1, container2};
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

template<typename Container, typename T>
bool contains_that(Container&& container, T op)
{
    return find_if(container, op) != end(container);
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
