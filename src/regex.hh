#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"
#include "string_utils.hh"
#include "exception.hh"
#include "utf8_iterator.hh"
#include "regex_impl.hh"

#include <boost/regex.hpp>

namespace Kakoune
{

struct regex_error : runtime_error
{
    regex_error(StringView desc)
        : runtime_error{format("regex error: '{}'", desc)}
    {}
};

using RegexBase = boost::basic_regex<wchar_t, boost::c_regex_traits<wchar_t>>;

// Regex that keeps track of its string representation
class Regex : public RegexBase
{
public:
    Regex() = default;

    explicit Regex(StringView re, flag_type flags = ECMAScript);
    bool empty() const { return m_str.empty(); }
    bool operator==(const Regex& other) const { return m_str == other.m_str; }
    bool operator!=(const Regex& other) const { return m_str != other.m_str; }

    const String& str() const { return m_str; }

    static constexpr const char* option_type_name = "regex";

    const CompiledRegex& impl() const { return m_impl; }

private:
    String m_str;
    CompiledRegex m_impl;
};

template<typename It>
using RegexUtf8It = utf8::iterator<It, wchar_t, ssize_t>;

template<typename It>
using RegexIteratorBase = boost::regex_iterator<RegexUtf8It<It>, wchar_t,
                                                boost::c_regex_traits<wchar_t>>;

namespace RegexConstant = boost::regex_constants;

template<typename Iterator>
struct MatchResults : boost::match_results<RegexUtf8It<Iterator>>
{
    using ParentType = boost::match_results<RegexUtf8It<Iterator>>;
    struct SubMatch : std::pair<Iterator, Iterator>
    {
        SubMatch() = default;
        SubMatch(const boost::sub_match<RegexUtf8It<Iterator>>& m)
            : std::pair<Iterator, Iterator>{m.first.base(), m.second.base()},
              matched{m.matched}
        {}

        bool matched = false;
    };

    struct iterator : boost::match_results<RegexUtf8It<Iterator>>::iterator
    {
        using ParentType = typename boost::match_results<RegexUtf8It<Iterator>>::iterator;
        iterator(const ParentType& it) : ParentType(it) {}

        SubMatch operator*() const { return {ParentType::operator*()}; }
    };

    iterator begin() const { return {ParentType::begin()}; }
    iterator cbegin() const { return {ParentType::cbegin()}; }
    iterator end() const { return {ParentType::end()}; }
    iterator cend() const { return {ParentType::cend()}; }

    SubMatch operator[](size_t s) const { return {ParentType::operator[](s)}; }
};

template<typename Iterator>
struct RegexIterator : RegexIteratorBase<Iterator>
{
    using Utf8It = RegexUtf8It<Iterator>;
    using ValueType = MatchResults<Iterator>;

    RegexIterator() = default;
    RegexIterator(Iterator begin, Iterator end, const Regex& re,
                  RegexConstant::match_flag_type flags = RegexConstant::match_default)
        : RegexIteratorBase<Iterator>{Utf8It{begin, begin, end}, Utf8It{end, begin, end}, re, flags} {}

    const ValueType& operator*() const { return *reinterpret_cast<const ValueType*>(&RegexIteratorBase<Iterator>::operator*()); }
    const ValueType* operator->() const { return reinterpret_cast<const ValueType*>(RegexIteratorBase<Iterator>::operator->()); }
};

inline RegexConstant::match_flag_type match_flags(bool bol, bool eol, bool bow, bool eow)
{
    return (bol ? RegexConstant::match_default : RegexConstant::match_not_bol) |
           (eol ? RegexConstant::match_default : RegexConstant::match_not_eol) |
           (bow ? RegexConstant::match_default : RegexConstant::match_not_bow) |
           (eow ? RegexConstant::match_default : RegexConstant::match_not_eow);
}

void regex_mismatch(const Regex& re);

template<typename It>
void check_captures(const Regex& re, const MatchResults<It>& res, const Vector<It>& captures)
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

inline RegexExecFlags convert_flags(RegexConstant::match_flag_type flags)
{
    auto res = RegexExecFlags::None;

    if (flags & RegexConstant::match_not_bol)
        res |= RegexExecFlags::NotBeginOfLine;
    if (flags & RegexConstant::match_not_eol)
        res |= RegexExecFlags::NotEndOfLine;
    if (flags & RegexConstant::match_not_bow)
        res |= RegexExecFlags::NotBeginOfWord;
    if (flags & RegexConstant::match_not_eow)
        res |= RegexExecFlags::NotEndOfWord;
    if (flags & RegexConstant::match_not_bob)
        res |= RegexExecFlags::NotBeginOfSubject;
    if (flags & RegexConstant::match_not_initial_null)
        res |= RegexExecFlags::NotInitialNull;
    if (flags & RegexConstant::match_any)
        res |= RegexExecFlags::AnyMatch;

    return res;
}

template<typename It>
bool regex_match(It begin, It end, const Regex& re)
{
    try
    {
        bool matched = boost::regex_match<RegexUtf8It<It>>({begin, begin, end}, {end, begin, end}, re);
        if (re.impl() and matched != regex_match(begin, end, re.impl()))
            regex_mismatch(re);
        return matched;
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex matching error: {}", err.what())};
    }
}

template<typename It>
bool regex_match(It begin, It end, MatchResults<It>& res, const Regex& re)
{
    try
    {
        bool matched = boost::regex_match<RegexUtf8It<It>>({begin, begin, end}, {end, begin, end}, res, re);
        Vector<It> captures;
        if (re.impl() and matched != regex_match(begin, end, captures, re.impl()))
            regex_mismatch(re);
        if (re.impl() and matched)
            check_captures(re, res, captures);
        return matched;
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex matching error: {}", err.what())};
    }
}

template<typename It>
bool regex_search(It begin, It end, const Regex& re,
                  RegexConstant::match_flag_type flags = RegexConstant::match_default)
{
    try
    {
        bool matched = boost::regex_search<RegexUtf8It<It>>({begin, begin, end}, {end, begin, end}, re, flags);
        if (re.impl() and matched != regex_search(begin, end, re.impl(), convert_flags(flags)))
            regex_mismatch(re);
        return matched;
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex searching error: {}", err.what())};
    }
}

template<typename It>
bool regex_search(It begin, It end, MatchResults<It>& res, const Regex& re,
                  RegexConstant::match_flag_type flags = RegexConstant::match_default)
{
    try
    {
        bool matched = boost::regex_search<RegexUtf8It<It>>({begin, begin, end}, {end, begin, end}, res, re, flags);
        Vector<It> captures;
        if (re.impl() and matched != regex_search(begin, end, captures, re.impl(), convert_flags(flags)))
            regex_mismatch(re);
        if (re.impl() and matched)
            check_captures(re, res, captures);
        return matched;
    }
    catch (std::runtime_error& err)
    {
        throw runtime_error{format("Regex searching error: {}", err.what())};
    }
}

String option_to_string(const Regex& re);
void option_from_string(StringView str, Regex& re);

}

#endif // regex_hh_INCLUDED
