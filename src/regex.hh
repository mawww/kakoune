#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"
#include "regex_impl.hh"

#define REGEX_CHECK_WITH_BOOST

#ifdef REGEX_CHECK_WITH_BOOST
#include "exception.hh"
#include "string_utils.hh"
#include "utf8_iterator.hh"
#include <boost/regex.hpp>
#endif

namespace Kakoune
{

// Regex that keeps track of its string representation
class Regex
{
public:
    Regex() = default;

    explicit Regex(StringView re, RegexCompileFlags flags = RegexCompileFlags::None,
                   MatchDirection direction = MatchDirection::Forward);
    bool empty() const { return m_str.empty(); }
    bool operator==(const Regex& other) const { return m_str == other.m_str; }
    bool operator!=(const Regex& other) const { return m_str != other.m_str; }

    const String& str() const { return m_str; }

    size_t mark_count() const { return m_impl->save_count / 2 - 1; }

    static constexpr const char* option_type_name = "regex";

    const CompiledRegex* impl() const { return m_impl.get(); }

#ifdef REGEX_CHECK_WITH_BOOST
    using BoostImpl = boost::basic_regex<wchar_t, boost::c_regex_traits<wchar_t>>;
    const BoostImpl& boost_impl() const { return m_boost_impl; }
#endif

private:
    RefPtr<CompiledRegex> m_impl;
    String m_str;
#ifdef REGEX_CHECK_WITH_BOOST
    BoostImpl m_boost_impl;
#endif
};

template<typename Iterator>
struct MatchResults
{
    struct SubMatch : std::pair<Iterator, Iterator>
    {
        SubMatch() = default;
        SubMatch(Iterator begin, Iterator end)
            : std::pair<Iterator, Iterator>{begin, end}, matched{begin != Iterator{}}
        {}

        bool matched = false;
    };

    struct iterator : std::iterator<std::bidirectional_iterator_tag, SubMatch, size_t, SubMatch*, SubMatch>
    {
        using It = typename Vector<Iterator>::const_iterator;

        iterator() = default;
        iterator(It it) : m_it{std::move(it)} {}

        iterator& operator--() { m_it += 2; return *this; }
        iterator& operator++() { m_it += 2; return *this; }
        SubMatch operator*() const { return {*m_it, *(m_it+1)}; }

        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.m_it == rhs.m_it; }
        friend bool operator!=(const iterator& lhs, const iterator& rhs) { return lhs.m_it != rhs.m_it; }
    private:

        It m_it;
    };

    MatchResults() = default;
    MatchResults(Vector<Iterator> values) : m_values{std::move(values)} {}

    iterator begin() const { return iterator{m_values.begin()}; }
    iterator cbegin() const { return iterator{m_values.cbegin()}; }
    iterator end() const { return iterator{m_values.end()}; }
    iterator cend() const { return iterator{m_values.cend()}; }

    size_t size() const { return m_values.size() / 2; }
    bool empty() const { return m_values.empty(); }

    SubMatch operator[](size_t i) const
    {
        return i * 2 < m_values.size() ?
            SubMatch{m_values[i*2], m_values[i*2+1]} : SubMatch{};
    }

    friend bool operator==(const MatchResults& lhs, const MatchResults& rhs)
    {
        return lhs.m_values == rhs.m_values;
    }

    friend bool operator!=(const MatchResults& lhs, const MatchResults& rhs)
    {
        return not (lhs == rhs);
    }

    void swap(MatchResults& other)
    {
        m_values.swap(other.m_values);
    }

private:
    Vector<Iterator> m_values;
};

inline RegexExecFlags match_flags(bool bol, bool eol, bool bow, bool eow)
{
    return (bol ? RegexExecFlags::None : RegexExecFlags::NotBeginOfLine) |
           (eol ? RegexExecFlags::None : RegexExecFlags::NotEndOfLine) |
           (bow ? RegexExecFlags::None : RegexExecFlags::NotBeginOfWord) |
           (eow ? RegexExecFlags::None : RegexExecFlags::NotEndOfWord);
}

#ifdef REGEX_CHECK_WITH_BOOST
void regex_mismatch(const Regex& re);

template<typename It>
using RegexUtf8It = utf8::iterator<It, wchar_t, ssize_t>;

template<typename It>
void check_captures(const Regex& re, const boost::match_results<RegexUtf8It<It>>& res, const Vector<It>& captures)
{
    if (res.size() > captures.size() * 2)
        return regex_mismatch(re);

    for (size_t i = 0; i < res.size(); ++i)
    {
        if (not res[i].matched)
        {
            if (captures[i*2] != It{} or captures[i*2+1] != It{})
                regex_mismatch(re);
            continue;
        }

        if (res[i].first != captures[i*2])
            regex_mismatch(re);
        if (res[i].second != captures[i*2+1])
            regex_mismatch(re);
    }
}

boost::regbase::flag_type convert_flags(RegexCompileFlags flags);
boost::regex_constants::match_flag_type convert_flags(RegexExecFlags flags);
#endif

template<typename It>
bool regex_match(It begin, It end, const Regex& re)
{
    const bool matched = regex_match(begin, end, *re.impl());
#ifdef REGEX_CHECK_WITH_BOOST
    try
    {
        if (not re.boost_impl().empty() and
            matched != boost::regex_match<RegexUtf8It<It>>({begin, begin, end}, {end, begin, end},
                                                           re.boost_impl()))
            regex_mismatch(re);
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex matching error: {}", err.what())};
    }
#endif
    return matched;
}

template<typename It>
bool regex_match(It begin, It end, MatchResults<It>& res, const Regex& re)
{
    Vector<It> captures;
    const bool matched = regex_match(begin, end, captures, *re.impl());

#ifdef REGEX_CHECK_WITH_BOOST
    try
    {
        boost::match_results<RegexUtf8It<It>> boost_res;
        if (not re.boost_impl().empty() and
            matched != boost::regex_match<RegexUtf8It<It>>({begin, begin, end}, {end, begin, end},
                                                           boost_res, re.boost_impl()))
            regex_mismatch(re);
        if (not re.boost_impl().empty() and matched)
            check_captures(re, boost_res, captures);
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex matching error: {}", err.what())};
    }
#endif

    res = matched ? MatchResults<It>{std::move(captures)} : MatchResults<It>{};
    return matched;
}

template<typename It>
bool regex_search(It begin, It end, const Regex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    const bool matched = regex_search(begin, end, *re.impl(), flags);

#ifdef REGEX_CHECK_WITH_BOOST
    try
    {
        auto first = (flags & RegexExecFlags::PrevAvailable) ? begin-1 : begin;
        if (not re.boost_impl().empty() and
            matched != boost::regex_search<RegexUtf8It<It>>({begin, first, end}, {end, first, end},
                                                            re.boost_impl(), convert_flags(flags)))
            regex_mismatch(re);
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex searching error: {}", err.what())};
    }
#endif
    return matched;
}

template<typename It, MatchDirection direction = MatchDirection::Forward>
bool regex_search(It begin, It end, MatchResults<It>& res, const Regex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    Vector<It> captures;
    const bool matched = regex_search<It, direction>(begin, end, captures, *re.impl(), flags);

#ifdef REGEX_CHECK_WITH_BOOST
    try
    {
        if (direction == MatchDirection::Forward)
        {
            auto first = (flags & RegexExecFlags::PrevAvailable) ? begin-1 : begin;
            boost::match_results<RegexUtf8It<It>> boost_res;
            if (not re.boost_impl().empty() and
                matched != boost::regex_search<RegexUtf8It<It>>({begin, first, end}, {end, first, end},
                                                                boost_res, re.boost_impl(), convert_flags(flags)))
                regex_mismatch(re);
            if (not re.boost_impl().empty() and matched)
                check_captures(re, boost_res, captures);
        }
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex searching error: {}", err.what())};
    }
#endif

    res = matched ? MatchResults<It>{std::move(captures)} : MatchResults<It>{};
    return matched;
}

String option_to_string(const Regex& re);
void option_from_string(StringView str, Regex& re);

template<typename Iterator>
struct RegexIterator
{
    using ValueType = MatchResults<Iterator>;

    RegexIterator() = default;
    RegexIterator(Iterator begin, Iterator end, const Regex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
        : m_regex{&re}, m_next_begin{begin}, m_begin{begin}, m_end{end}, m_flags{flags}
    {
        next();
    }

    const ValueType& operator*() const { kak_assert(m_regex); return m_results; }
    const ValueType* operator->() const { kak_assert(m_regex); return &m_results; }

    RegexIterator& operator++()
    {
        next();
        return *this;
    }

    friend bool operator==(const RegexIterator& lhs, const RegexIterator& rhs)
    {
        if (lhs.m_regex == nullptr and rhs.m_regex == nullptr)
            return true;

        return lhs.m_regex == rhs.m_regex and
               lhs.m_next_begin == rhs.m_next_begin and
               lhs.m_end == rhs.m_end and
               lhs.m_flags == rhs.m_flags and
               lhs.m_results == rhs.m_results;
    }

    friend bool operator!=(const RegexIterator& lhs, const RegexIterator& rhs)
    {
        return not (lhs == rhs);
    }

private:
    void next()
    {
        kak_assert(m_regex);

        RegexExecFlags additional_flags{};
        if (m_results.size() and m_results[0].first == m_results[0].second)
            additional_flags |= RegexExecFlags::NotInitialNull;
        if (m_begin != m_next_begin)
            additional_flags |= RegexExecFlags::NotBeginOfSubject | RegexExecFlags::PrevAvailable;

        if (not regex_search(m_next_begin, m_end, m_results, *m_regex,
                             m_flags | additional_flags))
            m_regex = nullptr;
        else
            m_next_begin = m_results[0].second;
    }

    const Regex* m_regex = nullptr;
    MatchResults<Iterator> m_results;
    Iterator m_next_begin{};
    const Iterator m_begin{};
    const Iterator m_end{};
    const RegexExecFlags m_flags = RegexExecFlags::None;
};

}

#endif // regex_hh_INCLUDED
