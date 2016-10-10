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

    iterator begin() { return m_container.rbegin(); }
    iterator end()   { return m_container.rend(); }

    Container m_container;
};

template<typename C>
using RemoveReference = typename std::remove_reference<C>::type;

struct ReverseFactory
{
    template<typename Container>
    ReverseView<RemoveReference<Container>> operator()(Container&& container) const
    {
        return {std::move(container)};
    }

    template<typename Container>
    ReverseView<Container&> operator()(Container& container) const
    {
        return {container};
    }
};

inline ContainerView<ReverseFactory> reverse() { return {}; }

template<typename Container>
using IteratorOf = decltype(std::begin(std::declval<Container>()));

template<typename Container>
using ValueOf = typename Container::value_type;

template<typename Container, typename Filter>
struct FilterView
{
    using ContainerIt = IteratorOf<Container>;

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

    Iterator begin() const { return {*this, m_container.begin(), m_container.end()}; }
    Iterator end()   const { return {*this, m_container.end(), m_container.end()}; }

    Container m_container;
    Filter m_filter;
};

template<typename Filter>
struct FilterFactory
{
    template<typename Container>
    FilterView<Container&, Filter> operator()(Container& container) const { return {container, std::move(m_filter)}; }

    template<typename Container>
    FilterView<RemoveReference<Container>, Filter> operator()(Container&& container) const { return {std::move(container), std::move(m_filter)}; }

    Filter m_filter;
};

template<typename Filter>
inline ContainerView<FilterFactory<Filter>> filter(Filter f) { return {{std::move(f)}}; }

template<typename I, typename T>
using TransformedResult = decltype(std::declval<T>()(*std::declval<I>()));

template<typename Container, typename Transform>
struct TransformView
{
    using ContainerIt = IteratorOf<Container>;

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

    Iterator begin() const { return {*this, m_container.begin()}; }
    Iterator end()   const { return {*this, m_container.end()}; }

    Container m_container;
    Transform m_transform;
};

template<typename Transform>
struct TransformFactory
{
    template<typename Container>
    TransformView<Container&, Transform> operator()(Container& container) const { return {container, std::move(m_transform)}; }

    template<typename Container>
    TransformView<RemoveReference<Container>, Transform> operator()(Container&& container) const { return {std::move(container), std::move(m_transform)}; }

    Transform m_transform;
};

template<typename Transform>
inline ContainerView<TransformFactory<Transform>> transform(Transform t) { return {{std::move(t)}}; }

template<typename Container, typename Separator = ValueOf<Container>,
         typename ValueTypeParam = void>
struct SplitView
{
    using ContainerIt = IteratorOf<Container>;
    using ValueType = typename std::conditional<std::is_same<void, ValueTypeParam>::value,
                                                std::pair<IteratorOf<Container>, IteratorOf<Container>>,
                                                ValueTypeParam>::type;

    struct Iterator : std::iterator<std::forward_iterator_tag, ValueType>
    {
        Iterator(ContainerIt pos, ContainerIt end, char separator)
         : pos(pos), sep(pos), end(end), separator(separator)
        {
            while (sep != end and *sep != separator)
                ++sep;
        }

        Iterator& operator++() { advance(); return *this; }
        Iterator operator++(int) { auto copy = *this; advance(); return copy; }

        bool operator==(const Iterator& other) const { return pos == other.pos; }
        bool operator!=(const Iterator& other) const { return pos != other.pos; }

        ValueType operator*() { return {pos, sep}; }

    private:
        void advance()
        {
            if (sep == end)
            {
                pos = end;
                return;
            }

            pos = sep+1;
            for (sep = pos; sep != end; ++sep)
            {
                if (*sep == separator)
                    break;
            }
        }

        ContainerIt pos;
        ContainerIt sep;
        ContainerIt end;
        Separator separator;
    };

    Iterator begin() const { return {m_container.begin(), m_container.end(), m_separator}; }
    Iterator end()   const { return {m_container.end(), m_container.end(), m_separator}; }

    Container m_container;
    Separator m_separator;
};

template<typename ValueType, typename Separator>
struct SplitViewFactory
{
    template<typename Container>
    SplitView<RemoveReference<Container>, Separator, ValueType>
    operator()(Container&& container) const { return {std::move(container), std::move(separator)}; }

    template<typename Container>
    SplitView<Container&, Separator, ValueType>
    operator()(Container& container) const { return {container, std::move(separator)}; }

    Separator separator;
};

template<typename ValueType = void, typename Separator>
ContainerView<SplitViewFactory<ValueType, Separator>> split(Separator separator) { return {{std::move(separator)}}; }

template<typename Container1, typename Container2>
struct ConcatView
{
    using ContainerIt1 = decltype(begin(std::declval<Container1>()));
    using ContainerIt2 = decltype(begin(std::declval<Container2>()));
    using ValueType = typename std::common_type<typename ContainerIt1::value_type, typename ContainerIt2::value_type>::type;

    struct Iterator : std::iterator<std::forward_iterator_tag, ValueType>
    {
        static_assert(std::is_convertible<typename ContainerIt1::value_type, ValueType>::value, "");
        static_assert(std::is_convertible<typename ContainerIt2::value_type, ValueType>::value, "");

        Iterator(ContainerIt1 it1, ContainerIt1 end1, ContainerIt2 it2)
            : m_it1(std::move(it1)), m_end1(std::move(end1)),
              m_it2(std::move(it2)) {}

        ValueType operator*() { return is2() ? *m_it2 : *m_it1; }
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
        : m_container1(container1), m_container2(container2) {}

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
