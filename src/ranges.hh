#ifndef ranges_hh_INCLUDED
#define ranges_hh_INCLUDED

#include <algorithm>
#include <utility>
#include <iterator>
#include <numeric>

#include "constexpr_utils.hh"

namespace Kakoune
{

template<typename Func> struct ViewFactory { Func func; };

template<typename Func>
ViewFactory<std::decay_t<Func>>
make_view_factory(Func&& func) { return {std::forward<Func>(func)}; }

template<typename Range, typename Func>
decltype(auto) operator| (Range&& range, ViewFactory<Func> factory)
{
    return factory.func(std::forward<Range>(range));
}

template<typename Range>
struct decay_range_impl { using type = std::decay_t<Range>; };

template<typename Range>
struct decay_range_impl<Range&> { using type = Range&; };

template<typename Range>
using decay_range = typename decay_range_impl<Range>::type;

template<typename Range>
struct ReverseView
{
    decltype(auto) begin() { return m_range.rbegin(); }
    decltype(auto) end()   { return m_range.rend(); }

    Range m_range;
};

inline auto reverse()
{
    return make_view_factory([](auto&& range) {
        using Range = decltype(range);
        return ReverseView<decay_range<Range>>{std::forward<Range>(range)};
    });
}

template<typename Range>
using IteratorOf = decltype(std::begin(std::declval<Range>()));

template<typename Range>
using ValueOf = typename Range::value_type;

template<typename Range>
struct SkipView
{
    auto begin() const { return std::next(std::begin(m_range), m_skip_count); }
    auto end()   const { return std::end(m_range); }

    Range m_range;
    size_t m_skip_count;
};

inline auto skip(size_t count)
{
    return make_view_factory([count](auto&& range) {
        using Range = decltype(range);
        return SkipView<decay_range<Range>>{std::forward<Range>(range), count};
    });
}

template<typename Range, typename Filter>
struct FilterView
{
    using RangeIt = IteratorOf<Range>;

    struct Iterator : std::iterator<std::forward_iterator_tag,
                                    typename std::iterator_traits<RangeIt>::value_type>
    {
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
inline auto filter(Filter f)
{
    return make_view_factory([f = std::move(f)](auto&& range) {
        using Range = decltype(range);
        return FilterView<decay_range<Range>, Filter>{std::forward<Range>(range), std::move(f)};
    });
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

        Iterator operator+(difference_type diff) { return {*m_transform, m_it + diff}; }
        Iterator operator-(difference_type diff) { return {*m_transform, m_it - diff}; }

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
inline auto transform(Transform t)
{
    return make_view_factory([t = std::move(t)](auto&& range) {
        using Range = decltype(range);
        return TransformView<decay_range<Range>, Transform>{std::forward<Range>(range), std::move(t)};
    });
}

template<typename T, typename U, typename = void>
struct is_pointer_like : std::false_type {};

template<typename T, typename U>
struct is_pointer_like<T, U, std::enable_if_t<std::is_same_v<std::decay_t<decltype(*std::declval<U>())>, std::decay_t<T>>>> : std::true_type {};

template<typename M, typename T>
inline auto transform(M T::*member)
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

    struct Iterator : std::iterator<std::forward_iterator_tag, ValueType>
    {
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
    return make_view_factory([s = std::move(separator)](auto&& range) {
        using Range = decltype(range);
        return SplitView<decay_range<Range>, false, false, Element, ValueType>{std::forward<Range>(range), std::move(s), {}};
    });
}

template<typename ValueType = void, typename Element>
auto split_after(Element separator)
{
    return make_view_factory([s = std::move(separator)](auto&& range) {
        using Range = decltype(range);
        return SplitView<decay_range<Range>, false, true, Element, ValueType>{std::forward<Range>(range), std::move(s), {}};
    });
}

template<typename ValueType = void, typename Element>
auto split(Element separator, Element escaper)
{
    return make_view_factory([s = std::move(separator), e = std::move(escaper)](auto&& range) {
        using Range = decltype(range);
        return SplitView<decay_range<Range>, true, false, Element, ValueType>{std::forward<Range>(range), std::move(s), std::move(e)};
    });
}

template<typename Range1, typename Range2>
struct ConcatView
{
    using RangeIt1 = decltype(std::declval<Range1>().begin());
    using RangeIt2 = decltype(std::declval<Range2>().begin());
    using ValueType = typename std::common_type_t<typename std::iterator_traits<RangeIt1>::value_type,
                                                  typename std::iterator_traits<RangeIt2>::value_type>;

    struct Iterator : std::iterator<std::forward_iterator_tag, ValueType>
    {
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

    ConcatView(Range1& range1, Range2& range2)
        : m_range1(range1), m_range2(range2) {}

    Iterator begin() const { return {m_range1.begin(), m_range1.end(), m_range2.begin()}; }
    Iterator end()   const { return {m_range1.end(), m_range1.end(), m_range2.end()}; }

private:
    Range1 m_range1;
    Range2 m_range2;
};

template<typename Range1, typename Range2>
ConcatView<decay_range<Range1>, decay_range<Range2>> concatenated(Range1&& range1, Range2&& range2)
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
    return make_view_factory([](auto&& range) {
        using std::begin; using std::end;
        return Container(begin(range), end(range));
    });
}

template<typename ExceptionType, size_t... Indexes>
auto elements(bool exact_size = false)
{
    return make_view_factory([=] (auto&& range) {
        using std::begin; using std::end;
        auto it = begin(range), end_it = end(range);
        size_t i = 0;
        auto elem = [&](size_t index) {
            for (; i < index; ++i)
                if (++it == end_it) throw ExceptionType{i};
            return *it;
        };
        // Note that initializer lists elements are guaranteed to be sequenced
        Array<std::decay_t<decltype(*begin(range))>, sizeof...(Indexes)> res{{elem(Indexes)...}};
        if (exact_size and ++it != end_it)
            throw ExceptionType{++i};
        return res;
    });
}

template<typename ExceptionType, size_t... Indexes>
auto static_gather_impl(std::index_sequence<Indexes...>)
{
    return elements<ExceptionType, Indexes...>(true);
}

template<typename ExceptionType, size_t size>
auto static_gather()
{
    return static_gather_impl<ExceptionType>(std::make_index_sequence<size>());
}

}

#endif // ranges_hh_INCLUDED
