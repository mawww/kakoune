#ifndef containers_hh_INCLUDED
#define containers_hh_INCLUDED

#include <algorithm>
#include <utility>
#include <iterator>
#include <numeric>

namespace Kakoune
{

template<typename Func> struct ViewFactory { Func func; };

template<typename Func>
ViewFactory<std::decay_t<Func>>
make_view_factory(Func&& func) { return {std::forward<Func>(func)}; }

template<typename Container, typename Func>
decltype(auto) operator| (Container&& container, ViewFactory<Func> factory)
{
    return factory.func(std::forward<Container>(container));
}

template<typename Container>
struct decay_container_impl { using type = std::decay_t<Container>; };

template<typename Container>
struct decay_container_impl<Container&> { using type = Container&; };

template<typename Container>
using decay_container = typename decay_container_impl<Container>::type;

template<typename Container>
struct ReverseView
{
    decltype(auto) begin() { return m_container.rbegin(); }
    decltype(auto) end()   { return m_container.rend(); }

    Container m_container;
};

inline auto reverse()
{
    return make_view_factory([](auto&& container) {
        using Container = decltype(container);
        return ReverseView<decay_container<Container>>{std::forward<Container>(container)};
    });
}

template<typename Container>
using IteratorOf = decltype(std::begin(std::declval<Container>()));

template<typename Container>
using ValueOf = typename Container::value_type;

template<typename Container, typename Filter>
struct FilterView
{
    using ContainerIt = IteratorOf<Container>;

    struct Iterator : std::iterator<std::forward_iterator_tag,
                                    typename std::iterator_traits<ContainerIt>::value_type>
    {
        Iterator(const FilterView& view, ContainerIt it, ContainerIt end)
            : m_it{std::move(it)}, m_end{std::move(end)}, m_view{view}
        {
            do_filter();
        }

        decltype(auto) operator*() { return *m_it; }
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

    Iterator begin() const { return {*this, std::begin(m_container), std::end(m_container)}; }
    Iterator end()   const { return {*this, std::end(m_container), std::end(m_container)}; }

    Container m_container;
    mutable Filter m_filter;
};

template<typename Filter>
inline auto filter(Filter f)
{
    return make_view_factory([f = std::move(f)](auto&& container) {
        using Container = decltype(container);
        return FilterView<decay_container<Container>, Filter>{std::forward<Container>(container), std::move(f)};
    });
}

template<typename Container, typename Transform>
struct TransformView
{
    using ContainerIt = IteratorOf<Container>;
    using ResType = decltype(std::declval<Transform>()(*std::declval<ContainerIt>()));

    struct Iterator : std::iterator<std::forward_iterator_tag, std::remove_reference_t<ResType>>
    {
        Iterator(const TransformView& view, ContainerIt it)
            : m_it{std::move(it)}, m_view{view} {}

        decltype(auto) operator*() { return m_view.m_transform(*m_it); }
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

    Iterator begin() const { return {*this, std::begin(m_container)}; }
    Iterator end()   const { return {*this, std::end(m_container)}; }

    Container m_container;
    mutable Transform m_transform;
};

template<typename Transform>
inline auto transform(Transform t)
{
    return make_view_factory([t = std::move(t)](auto&& container) {
        using Container = decltype(container);
        return TransformView<decay_container<Container>, Transform>{std::forward<Container>(container), std::move(t)};
    });
}

template<typename Container, typename Separator = ValueOf<Container>,
         typename ValueTypeParam = void>
struct SplitView
{
    using ContainerIt = IteratorOf<Container>;
    using ValueType = std::conditional_t<std::is_same<void, ValueTypeParam>::value,
                                         std::pair<IteratorOf<Container>, IteratorOf<Container>>,
                                         ValueTypeParam>;

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

    Iterator begin() const { return {std::begin(m_container), std::end(m_container), m_separator}; }
    Iterator end()   const { return {std::end(m_container), std::end(m_container), m_separator}; }

    Container m_container;
    Separator m_separator;
};

template<typename ValueType = void, typename Separator>
auto split(Separator separator)
{
    return make_view_factory([s = std::move(separator)](auto&& container) {
        using Container = decltype(container);
        return SplitView<decay_container<Container>, Separator, ValueType>{std::forward<Container>(container), std::move(s)};
    });
}

template<typename Container1, typename Container2>
struct ConcatView
{
    using ContainerIt1 = decltype(begin(std::declval<Container1>()));
    using ContainerIt2 = decltype(begin(std::declval<Container2>()));
    using ValueType = typename std::common_type_t<typename std::iterator_traits<ContainerIt1>::value_type,
                                                  typename std::iterator_traits<ContainerIt2>::value_type>;

    struct Iterator : std::iterator<std::forward_iterator_tag, ValueType>
    {
        static_assert(std::is_convertible<typename std::iterator_traits<ContainerIt1>::value_type, ValueType>::value, "");
        static_assert(std::is_convertible<typename std::iterator_traits<ContainerIt2>::value_type, ValueType>::value, "");

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

template<typename Container, typename T>
auto find(Container&& container, const T& value)
{
    using std::begin; using std::end;
    return std::find(begin(container), end(container), value);
}

template<typename Container, typename T>
auto find_if(Container&& container, T op)
{
    using std::begin; using std::end;
    return std::find_if(begin(container), end(container), op);
}

template<typename Container, typename T>
bool contains(Container&& container, const T& value)
{
    using std::end;
    return find(container, value) != end(container);
}

template<typename Container, typename T>
bool contains_that(Container&& container, T op)
{
    using std::end;
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

template<typename Container, typename Init, typename BinOp>
Init accumulate(Container&& c, Init&& init, BinOp&& op)
{
    using std::begin; using std::end;
    return std::accumulate(begin(c), end(c), init, op);
}

}

#endif // containers_hh_INCLUDED
