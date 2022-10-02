#ifndef ranges_hh_INCLUDED
#define ranges_hh_INCLUDED

#include <algorithm>
#include <utility>
#include <iterator>
#include <numeric>
#include <tuple>

#include "constexpr_utils.hh"

namespace Kakoune
{

template<typename Func> struct ViewFactory { Func func; };

template<typename Func>
ViewFactory(Func&&) -> ViewFactory<std::remove_cvref_t<Func>>;

template<typename Range, typename Func>
decltype(auto) operator| (Range&& range, ViewFactory<Func> factory)
{
    return factory.func(std::forward<Range>(range));
}

template<typename Range>
struct DecayRangeImpl { using type = std::remove_cvref_t<Range>; };

template<typename Range>
struct DecayRangeImpl<Range&> { using type = Range&; };

template<typename Range>
using DecayRange = typename DecayRangeImpl<Range>::type;

template<typename Range>
struct RangeHolderImpl { using type = std::remove_cvref_t<Range>; };

template<typename Range>
struct RangeHolderImpl<Range&> {
    struct type
    {
        Range* range{};

        decltype(auto) begin() { return std::begin(*range); }
        decltype(auto) end() { return std::end(*range); }

        type& operator=(Range& r) { range = &r; return *this; }
        operator Range&() { return *range; }
    };
};

template<typename Range>
using RangeHolder = typename RangeHolderImpl<Range>::type;

template<typename Range>
struct ReverseView
{
    decltype(auto) begin() { return m_range.rbegin(); }
    decltype(auto) end()   { return m_range.rend(); }
    decltype(auto) rbegin() { return m_range.begin(); }
    decltype(auto) rend()   { return m_range.end(); }
    decltype(auto) begin() const { return m_range.rbegin(); }
    decltype(auto) end() const  { return m_range.rend(); }
    decltype(auto) rbegin() const { return m_range.begin(); }
    decltype(auto) rend() const  { return m_range.end(); }

    Range m_range;
};

constexpr auto reverse()
{
    return ViewFactory{[](auto&& range) {
        using Range = decltype(range);
        return ReverseView<DecayRange<Range>>{std::forward<Range>(range)};
    }};
}

template<typename Range>
using IteratorOf = decltype(std::begin(std::declval<Range>()));

template<typename Range>
using ValueOf = decltype(*std::declval<IteratorOf<Range>>());

template<typename Range>
struct SkipView
{
    auto begin() const { return std::next(std::begin(m_range), m_skip_count); }
    auto end()   const { return std::end(m_range); }

    Range m_range;
    size_t m_skip_count;
};

constexpr auto skip(size_t count)
{
    return ViewFactory{[count](auto&& range) {
        using Range = decltype(range);
        return SkipView<DecayRange<Range>>{std::forward<Range>(range), count};
    }};
}

template<typename Range>
struct DropView
{
    auto begin() const { return std::begin(m_range); }
    auto end()   const { return std::end(m_range) - m_drop_count; }

    Range m_range;
    size_t m_drop_count;
};

constexpr auto drop(size_t count)
{
    return ViewFactory{[count](auto&& range) {
        using Range = decltype(range);
        return DropView<DecayRange<Range>>{std::forward<Range>(range), count};
    }};
}

template<typename Range, typename Filter>
struct FilterView
{
    using RangeIt = IteratorOf<Range>;

    struct Iterator
    {
        using difference_type = ptrdiff_t;
        using value_type = typename std::iterator_traits<RangeIt>::value_type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::forward_iterator_tag;

        Iterator(Filter& filter, RangeIt it, RangeIt end)
            : m_it{std::move(it)}, m_end{std::move(end)}, m_filter{&filter}
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

        const RangeIt& base() const { return m_it; }

    private:
        void do_filter()
        {
            while (m_it != m_end and not (*m_filter)(*m_it))
                ++m_it;
        }

        RangeIt m_it;
        RangeIt m_end;
        Filter* m_filter;
    };

    Iterator begin() const { return {m_filter, std::begin(m_range), std::end(m_range)}; }
    Iterator end()   const { return {m_filter, std::end(m_range), std::end(m_range)}; }

    Range m_range;
    mutable Filter m_filter;
};

template<typename Filter>
constexpr auto filter(Filter f)
{
    return ViewFactory{[f = std::move(f)](auto&& range) {
        using Range = decltype(range);
        return FilterView<DecayRange<Range>, Filter>{std::forward<Range>(range), std::move(f)};
    }};
}

template<typename Range>
struct EnumerateView
{
    using RangeIt = IteratorOf<Range>;

    struct Iterator
    {
        using difference_type = ptrdiff_t;
        using value_type = typename std::iterator_traits<RangeIt>::value_type;
        using pointer = value_type*;
        using reference = value_type&;
        using iterator_category = std::forward_iterator_tag;

        Iterator(size_t index, RangeIt it)
            : m_index{index}, m_it{std::move(it)} {}

        decltype(auto) operator*() { return std::tuple<size_t, decltype(*m_it)>(m_index, *m_it); }
        Iterator& operator++() { ++m_index; ++m_it; return *this; }
        Iterator operator++(int) { auto copy = *this; ++(*this); return copy; }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs)
        {
            return lhs.m_it == rhs.m_it;
        }

        friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
        {
            return not (lhs == rhs);
        }

        const RangeIt& base() const { return m_it; }

    private:
        size_t m_index;
        RangeIt m_it;
    };

    Iterator begin() const { return {0, std::begin(m_range)}; }
    Iterator end()   const { return {(size_t)-1, std::end(m_range)}; }

    Range m_range;
};

constexpr auto enumerate()
{
    return ViewFactory{[](auto&& range) {
        using Range = decltype(range);
        return EnumerateView<DecayRange<Range>>{std::forward<Range>(range)};
    }};
}

template<typename Range, typename Transform>
struct TransformView
{
    using RangeIt = IteratorOf<Range>;
    using ResType = decltype(std::declval<Transform>()(*std::declval<RangeIt>()));

    struct Iterator
    {
        using iterator_category = typename std::iterator_traits<RangeIt>::iterator_category;
        using value_type = std::remove_reference_t<ResType>;
        using difference_type = typename std::iterator_traits<RangeIt>::difference_type;
        using pointer = value_type*;
        using reference = value_type&;

        Iterator(Transform& transform, RangeIt it)
            : m_it{std::move(it)}, m_transform{&transform} {}

        decltype(auto) operator*() { return (*m_transform)(*m_it); }
        decltype(auto) operator[](difference_type i) const { return (*m_transform)(m_it[i]); }

        Iterator& operator++() { ++m_it; return *this; }
        Iterator operator++(int) { auto copy = *this; ++m_it; return copy; }

        Iterator& operator--() { --m_it; return *this; }
        Iterator operator--(int) { auto copy = *this; --m_it; return copy; }

        Iterator& operator+=(difference_type diff) { m_it += diff; return *this; }
        Iterator& operator-=(difference_type diff) { m_it -= diff; return *this; }

        Iterator operator+(difference_type diff) const { return {*m_transform, m_it + diff}; }
        Iterator operator-(difference_type diff) const { return {*m_transform, m_it - diff}; }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs) { return lhs.m_it == rhs.m_it; }
        friend bool operator!=(const Iterator& lhs, const Iterator& rhs) { return not (lhs == rhs); }
        friend difference_type operator-(const Iterator& lhs, const Iterator& rhs) { return lhs.m_it - rhs.m_it; }

        RangeIt base() const { return m_it; }

    private:
        RangeIt m_it;
        Transform* m_transform;
    };

    Iterator begin() const { return {m_transform, std::begin(m_range)}; }
    Iterator end()   const { return {m_transform, std::end(m_range)}; }

    Range m_range;
    mutable Transform m_transform;
};

template<typename Transform>
constexpr auto transform(Transform t)
{
    return ViewFactory{[t = std::move(t)](auto&& range) {
        using Range = decltype(range);
        return TransformView<DecayRange<Range>, Transform>{std::forward<Range>(range), std::move(t)};
    }};
}

template<typename T, typename U>
struct is_pointer_like : std::false_type {};

template<typename T, typename U> requires std::is_same_v<std::remove_cvref_t<decltype(*std::declval<U>())>, std::remove_cvref_t<T>>
struct is_pointer_like<T, U> : std::true_type {};

template<typename M, typename T>
constexpr auto transform(M T::*member)
{
    return transform([member](auto&& arg) -> decltype(auto) {
        using Arg = decltype(arg);
        using Member = decltype(member);

        auto get_object = [&] () mutable  -> decltype(auto) {
            if constexpr (is_pointer_like<T, Arg>::value)
                return *std::forward<Arg>(arg);
            else
                return std::forward<Arg>(arg);
        };

        if constexpr (std::is_member_function_pointer_v<Member>)
            return (get_object().*member)();
        else
            return get_object().*member;
    });
}

template<typename Range, bool escape, bool include_separator,
         typename Element = ValueOf<Range>,
         typename ValueTypeParam = void>
struct SplitView
{
    using RangeIt = IteratorOf<Range>;
    using ValueType = std::conditional_t<std::is_same<void, ValueTypeParam>::value,
                                         std::pair<IteratorOf<Range>, IteratorOf<Range>>,
                                         ValueTypeParam>;

    struct Iterator
    {
        using difference_type = ptrdiff_t;
        using value_type = ValueType;
        using pointer = ValueType*;
        using reference = ValueType&;
        using iterator_category = std::forward_iterator_tag;

        Iterator(RangeIt pos, const RangeIt& end, Element separator, Element escaper)
         : done{pos == end}, pos{pos}, sep{pos}, end(end), separator{std::move(separator)}, escaper{std::move(escaper)}
        {
            bool escaped = false;
            while (sep != end and (escaped or *sep != separator))
            {
                escaped = escape and not escaped and *sep == escaper;
                ++sep;
            }
        }

        Iterator& operator++() { advance(); return *this; }
        Iterator operator++(int) { auto copy = *this; advance(); return copy; }

        bool operator==(const Iterator& other) const { return pos == other.pos and done == other.done; }
        bool operator!=(const Iterator& other) const { return pos != other.pos or done != other.done; }

        ValueType operator*() { return {pos, (not include_separator or sep == end) ? sep : sep + 1}; }

    private:
        void advance()
        {
            if (sep == end)
            {
                pos = end;
                done = true;
                return;
            }

            pos = sep+1;
            if (include_separator and pos == end)
            {
                done = true;
                return;
            }
            bool escaped = escape and *sep == escaper;
            for (sep = pos; sep != end; ++sep)
            {
                if (not escaped and *sep == separator)
                    break;
                escaped = escape and not escaped and *sep == escaper;
            }
        }

        bool done;
        RangeIt pos;
        RangeIt sep;
        RangeIt end;
        Element separator;
        Element escaper;
    };

    Iterator begin() const { return {std::begin(m_range), std::end(m_range), m_separator, m_escaper}; }
    Iterator end()   const { return {std::end(m_range), std::end(m_range), m_separator, m_escaper}; }

    Range m_range;
    Element m_separator;
    Element m_escaper;
};

template<typename ValueType = void, typename Element>
auto split(Element separator)
{
    return ViewFactory{[s = std::move(separator)](auto&& range) {
        using Range = decltype(range);
        return SplitView<DecayRange<Range>, false, false, Element, ValueType>{std::forward<Range>(range), std::move(s), {}};
    }};
}

template<typename ValueType = void, typename Element>
auto split_after(Element separator)
{
    return ViewFactory{[s = std::move(separator)](auto&& range) {
        using Range = decltype(range);
        return SplitView<DecayRange<Range>, false, true, Element, ValueType>{std::forward<Range>(range), std::move(s), {}};
    }};
}

template<typename ValueType = void, typename Element>
auto split(Element separator, Element escaper)
{
    return ViewFactory{[s = std::move(separator), e = std::move(escaper)](auto&& range) {
        using Range = decltype(range);
        return SplitView<DecayRange<Range>, true, false, Element, ValueType>{std::forward<Range>(range), std::move(s), std::move(e)};
    }};
}

template<typename Range>
struct FlattenedView
{
    using OuterIt = IteratorOf<Range>;
    using InnerRange = ValueOf<Range>;
    using InnerIt = IteratorOf<InnerRange>;

    struct Iterator
    {
        using value_type = typename std::iterator_traits<InnerIt>::value_type;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::size_t;
        using reference = value_type&;
        using pointer = value_type*;

        Iterator() = default;
        Iterator(OuterIt begin, OuterIt end) : m_outer_it{begin}, m_outer_end{end}
        {
            find_next_inner();
        }

        decltype(auto) operator*() {  return *m_inner_it; }

        Iterator& operator++()
        {
            if (++m_inner_it == std::end(m_inner_range))
            {
                ++m_outer_it;
                find_next_inner();
            }
            return *this;
        }
        Iterator operator++(int) { auto copy = *this; ++*this; return copy; }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs)
        {
            return lhs.m_outer_it == rhs.m_outer_it and lhs.m_inner_it == rhs.m_inner_it;
        }

        void find_next_inner()
        {
            m_inner_it = InnerIt{};
            for (; m_outer_it != m_outer_end; ++m_outer_it)
            {
                m_inner_range = *m_outer_it;
                if (std::begin(m_inner_range) != std::end(m_inner_range))
                {
                    m_inner_it = std::begin(m_inner_range);
                    return;
                }
            }
        }

        OuterIt m_outer_it{};
        OuterIt m_outer_end{};
        InnerIt m_inner_it{};
        RangeHolder<InnerRange> m_inner_range;
    };

    Iterator begin() const { return {std::begin(m_range), std::end(m_range)}; }
    Iterator end()   const { return {std::end(m_range), std::end(m_range)}; }

    Range m_range;
};

constexpr auto flatten()
{
    return ViewFactory{[](auto&& range){
        using Range = decltype(range);
        return FlattenedView<DecayRange<Range>>{std::forward<Range>(range)};
    }};
}

template<typename Range1, typename Range2>
struct ConcatView
{
    using RangeIt1 = decltype(std::declval<Range1>().begin());
    using RangeIt2 = decltype(std::declval<Range2>().begin());
    using ValueType = typename std::common_type_t<typename std::iterator_traits<RangeIt1>::value_type,
                                                  typename std::iterator_traits<RangeIt2>::value_type>;

    struct Iterator
    {
        using difference_type = ptrdiff_t;
        using value_type = ValueType;
        using pointer = ValueType*;
        using reference = ValueType&;
        using iterator_category = std::forward_iterator_tag;

        static_assert(std::is_convertible<typename std::iterator_traits<RangeIt1>::value_type, ValueType>::value, "");
        static_assert(std::is_convertible<typename std::iterator_traits<RangeIt2>::value_type, ValueType>::value, "");

        Iterator(RangeIt1 it1, RangeIt1 end1, RangeIt2 it2)
            : m_it1(std::move(it1)), m_end1(std::move(end1)),
              m_it2(std::move(it2)) {}

        decltype(auto) operator*() { return is2() ? *m_it2 : *m_it1; }
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

        RangeIt1 m_it1;
        RangeIt1 m_end1;
        RangeIt2 m_it2;
    };

    Iterator begin() const { return {m_range1.begin(), m_range1.end(), m_range2.begin()}; }
    Iterator end()   const { return {m_range1.end(), m_range1.end(), m_range2.end()}; }

    Range1 m_range1;
    Range2 m_range2;
};

template<typename Range1, typename Range2>
ConcatView<DecayRange<Range1>, DecayRange<Range2>> concatenated(Range1&& range1, Range2&& range2)
{
    return {range1, range2};
}

template<typename Range, typename T>
auto find(Range&& range, const T& value)
{
    using std::begin; using std::end;
    return std::find(begin(range), end(range), value);
}

template<typename Range, typename T>
auto find_if(Range&& range, T op)
{
    using std::begin; using std::end;
    return std::find_if(begin(range), end(range), op);
}

template<typename Range, typename T>
bool contains(Range&& range, const T& value)
{
    using std::end;
    return find(range, value) != end(range);
}

template<typename Range, typename T>
bool all_of(Range&& range, T op)
{
    using std::begin; using std::end;
    return std::all_of(begin(range), end(range), op);
}

template<typename Range, typename T>
bool any_of(Range&& range, T op)
{
    using std::begin; using std::end;
    return std::any_of(begin(range), end(range), op);
}

template<typename Range, typename T>
auto remove_if(Range&& range, T op)
{
    using std::begin; using std::end;
    return std::remove_if(begin(range), end(range), op);
}

template<typename Range, typename U>
void unordered_erase(Range&& vec, U&& value)
{
    auto it = find(vec, std::forward<U>(value));
    if (it != vec.end())
    {
        using std::swap;
        swap(vec.back(), *it);
        vec.pop_back();
    }
}

template<typename Range, typename Init, typename BinOp>
Init accumulate(Range&& c, Init&& init, BinOp&& op)
{
    using std::begin; using std::end;
    return std::accumulate(begin(c), end(c), init, op);
}

template<typename Range, typename Compare, typename Func>
void for_n_best(Range&& c, size_t count, Compare&& compare, Func&& func)
{
    using std::begin; using std::end;
    auto b = begin(c), e = end(c);
    std::make_heap(b, e, compare);
    while (count > 0 and b != e)
    {
        if (func(*b))
            --count;
        std::pop_heap(b, e--, compare);
    }
}

template<typename Container>
auto gather()
{
    return ViewFactory{[](auto&& range) {
        using std::begin; using std::end;
        return Container(begin(range), end(range));
    }};
}

template<template <typename Element> class Container>
auto gather()
{
    return ViewFactory{[](auto&& range) {
        using std::begin; using std::end;
        using ValueType = std::remove_cv_t<std::remove_reference_t<decltype(*begin(range))>>;
        return Container<ValueType>(begin(range), end(range));
    }};
}

template<typename ExceptionType, bool exact_size, size_t... Indexes>
auto elements()
{
    return ViewFactory{[=] (auto&& range) {
        using std::begin; using std::end;
        auto it = begin(range), end_it = end(range);
        size_t i = 0;
        auto elem = [&](size_t index) {
            for (; i < index; ++i)
                if (++it == end_it) throw ExceptionType{i};
            return *it;
        };
        // Note that initializer lists elements are guaranteed to be sequenced
        Array<std::remove_cvref_t<decltype(*begin(range))>, sizeof...(Indexes)> res{{elem(Indexes)...}};
        if (exact_size and ++it != end_it)
            throw ExceptionType{++i};
        return res;
    }};
}

template<typename ExceptionType, bool exact_size, size_t... Indexes>
auto static_gather_impl(std::index_sequence<Indexes...>)
{
    return elements<ExceptionType, exact_size, Indexes...>();
}

template<typename ExceptionType, size_t size, bool exact_size = true>
auto static_gather()
{
    return static_gather_impl<ExceptionType, exact_size>(std::make_index_sequence<size>());
}

}

#endif // ranges_hh_INCLUDED
