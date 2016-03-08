#ifndef containers_hh_INCLUDED
#define containers_hh_INCLUDED

#include <algorithm>
#include <utility>
#include <iterator>

namespace Kakoune
{

template<typename Factory>
struct ContainerView { Factory factory; };

template<typename Container, typename Factory>
auto operator| (Container&& container, ContainerView<Factory> view) ->
    decltype(view.factory(std::forward<Container>(container)))
{
    return view.factory(std::forward<Container>(container));
}

template<typename Container>
struct ReverseView
{
    using iterator = decltype(std::declval<Container>().rbegin());
    ReverseView(Container& container) : m_container(container) {}

    iterator begin() { return m_container.rbegin(); }
    iterator end()   { return m_container.rend(); }

private:
    Container& m_container;
};

struct ReverseFactory
{
    template<typename Container>
    ReverseView<Container> operator()(Container&& container) const
    {
        return {container};
    }
};

inline ContainerView<ReverseFactory> reverse() { return {}; }

template<typename Container, typename Filter>
struct FilterView
{
    using ContainerIt = decltype(begin(std::declval<Container>()));

    struct Iterator : std::iterator<std::forward_iterator_tag,
                                    typename ContainerIt::value_type>
    {
        Iterator(const FilterView& view, ContainerIt it, ContainerIt end)
            : m_it{std::move(it)}, m_end{std::move(end)}, m_view{view}
        {
            do_filter();
        }

        auto operator*() -> decltype(*std::declval<ContainerIt>()) { return *m_it; }
        Iterator& operator++() { ++m_it; do_filter(); return *this; }
        Iterator operator++(int) { auto copy = *this; ++(*this); return copy; }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs)
        {
            return lhs.m_it == rhs.m_it;
        }

        friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
        {
            return not (lhs == rhs);
        }

        const ContainerIt& base() const { return m_it; }

    private:
        void do_filter()
        {
            while (m_it != m_end and not m_view.m_filter(*m_it))
                ++m_it;
        }

        ContainerIt m_it;
        ContainerIt m_end;
        const FilterView& m_view;
    };

    FilterView(Container& container, Filter filter)
        : m_container{container}, m_filter{std::move(filter)} {}

    Iterator begin() const { return {*this, m_container.begin(), m_container.end()}; }
    Iterator end()   const { return {*this, m_container.end(), m_container.end()}; }

private:
    Container& m_container;
    Filter m_filter;
};

template<typename Filter>
struct FilterFactory
{
    template<typename Container>
    FilterView<Container, Filter> operator()(Container&& container) const { return {container, std::move(m_filter)}; }

    Filter m_filter;
};

template<typename Filter>
inline ContainerView<FilterFactory<Filter>> filter(Filter f) { return {std::move(f)}; }

template<typename I, typename T>
using TransformedResult = decltype(std::declval<T>()(*std::declval<I>()));

template<typename Container, typename Transform>
struct TransformView
{
    using ContainerIt = decltype(begin(std::declval<Container>()));

    struct Iterator : std::iterator<std::forward_iterator_tag,
                                    typename std::remove_reference<TransformedResult<ContainerIt, Transform>>::type>
    {
        Iterator(const TransformView& view, ContainerIt it)
            : m_it{std::move(it)}, m_view{view} {}

        auto operator*() -> TransformedResult<ContainerIt, Transform> { return m_view.m_transform(*m_it); }
        Iterator& operator++() { ++m_it; return *this; }
        Iterator operator++(int) { auto copy = *this; ++m_it; return copy; }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs)
        {
            return lhs.m_it == rhs.m_it;
        }

        friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
        {
            return not (lhs == rhs);
        }

        ContainerIt base() const { return m_it; }

    private:
        ContainerIt m_it;
        const TransformView& m_view;
    };

    TransformView(Container& container, Transform transform)
        : m_container{container}, m_transform{std::move(transform)} {}

    Iterator begin() const { return {*this, m_container.begin()}; }
    Iterator end()   const { return {*this, m_container.end()}; }

private:
    Container& m_container;
    Transform m_transform;
};

template<typename Transform>
struct TransformFactory
{
    template<typename Container>
    TransformView<Container, Transform> operator()(Container&& container) const { return {container, std::move(m_transform)}; }

    Transform m_transform;
};

template<typename Transform>
inline ContainerView<TransformFactory<Transform>> transform(Transform t) { return {std::move(t)}; }



template<typename Container1, typename Container2>
struct ConcatView
{
    using ContainerIt1 = decltype(begin(std::declval<Container1>()));
    using ContainerIt2 = decltype(begin(std::declval<Container2>()));
    using ValueType = typename ContainerIt1::value_type;

    struct Iterator : std::iterator<std::forward_iterator_tag, ValueType>
    {
        static_assert(std::is_convertible<typename ContainerIt1::value_type, ValueType>::value, "");
        static_assert(std::is_convertible<typename ContainerIt2::value_type, ValueType>::value, "");

        Iterator(ContainerIt1 it1, ContainerIt1 end1, ContainerIt2 it2)
            : m_it1(std::move(it1)), m_end1(std::move(end1)),
              m_it2(std::move(it2)) {}

        decltype(*std::declval<ContainerIt1>()) operator*() { return is2() ? *m_it2 : *m_it1; }
        Iterator& operator++() { if (is2()) ++m_it2; else ++m_it1; return *this; }
        Iterator operator++(int) { auto copy = *this; ++*this; return copy; }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs)
        {
            return lhs.m_it1 == rhs.m_it1 and lhs.m_end1 == rhs.m_end1 and
                   lhs.m_it2 == rhs.m_it2;
        }

        friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
        {
            return not (lhs == rhs);
        }

    private:
        bool is2() const { return m_it1 == m_end1; }

        ContainerIt1 m_it1;
        ContainerIt1 m_end1;
        ContainerIt2 m_it2;
    };

    ConcatView(Container1& container1, Container2& container2)
        : m_container1{container1}, m_container2{container2} {}

    Iterator begin() const { return {m_container1.begin(), m_container1.end(), m_container2.begin()}; }
    Iterator end()   const { return {m_container1.end(), m_container1.end(), m_container2.end()}; }

private:
    Container1& m_container1;
    Container2& m_container2;
};

template<typename Container1, typename Container2>
ConcatView<Container1, Container2> concatenated(Container1&& container1, Container2&& container2)
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
