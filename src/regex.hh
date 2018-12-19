#ifndef regex_hh_INCLUDED
#define regex_hh_INCLUDED

#include "string.hh"
#include "regex_impl.hh"

namespace Kakoune
{

// Regex that keeps track of its string representation
class Regex
{
public:
    Regex() = default;

    explicit Regex(StringView re, RegexCompileFlags flags = RegexCompileFlags::None);
    bool empty() const { return m_str.empty(); }
    bool operator==(const Regex& other) const { return m_str == other.m_str; }
    bool operator!=(const Regex& other) const { return m_str != other.m_str; }

    const String& str() const { return m_str; }

    size_t mark_count() const { return m_impl->save_count / 2 - 1; }
    int named_capture_index(StringView name) const;

    static constexpr const char* option_type_name = "regex";

    const CompiledRegex* impl() const { return m_impl.get(); }

private:
    RefPtr<CompiledRegex> m_impl;
    String m_str;
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
        using It = typename Vector<Iterator, MemoryDomain::Regex>::const_iterator;

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
    MatchResults(Vector<Iterator, MemoryDomain::Regex> values) : m_values{std::move(values)} {}

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

    Vector<Iterator, MemoryDomain::Regex>& values() { return m_values; }

private:
    Vector<Iterator, MemoryDomain::Regex> m_values;
};

inline RegexExecFlags match_flags(bool bol, bool eol, bool bow, bool eow)
{
    return (bol ? RegexExecFlags::None : RegexExecFlags::NotBeginOfLine) |
           (eol ? RegexExecFlags::None : RegexExecFlags::NotEndOfLine) |
           (bow ? RegexExecFlags::None : RegexExecFlags::NotBeginOfWord) |
           (eow ? RegexExecFlags::None : RegexExecFlags::NotEndOfWord);
}

template<typename It>
bool regex_match(It begin, It end, const Regex& re)
{
    ThreadedRegexVM<It, MatchDirection::Forward> vm{*re.impl()};
    return vm.exec(begin, end, begin, end, RegexExecFlags::AnyMatch | RegexExecFlags::NoSaves);
}

template<typename It>
bool regex_match(It begin, It end, MatchResults<It>& res, const Regex& re)
{
    res.values().clear();
    ThreadedRegexVM<It, MatchDirection::Forward> vm{*re.impl()};
    if (vm.exec(begin, end, begin, end, RegexExecFlags::None))
    {
        std::copy(vm.captures().begin(), vm.captures().end(), std::back_inserter(res.values()));
        return true;
    }
    return false;
}

template<typename It>
bool regex_search(It begin, It end, It subject_begin, It subject_end, const Regex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It, MatchDirection::Forward> vm{*re.impl()};
    return vm.exec(begin, end, subject_begin, subject_end,
                   flags | RegexExecFlags::Search | RegexExecFlags::AnyMatch | RegexExecFlags::NoSaves);
}

template<typename It, MatchDirection direction = MatchDirection::Forward>
bool regex_search(It begin, It end, It subject_begin, It subject_end,
                  MatchResults<It>& res, const Regex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    res.values().clear();
    ThreadedRegexVM<It, direction> vm{*re.impl()};
    if (vm.exec(begin, end, subject_begin, subject_end, flags | RegexExecFlags::Search))
    {
        std::move(vm.captures().begin(), vm.captures().end(), std::back_inserter(res.values()));
        return true;
    }
    return false;
}

template<typename It>
bool backward_regex_search(It begin, It end, It subject_begin, It subject_end,
                           MatchResults<It>& res, const Regex& re,
                           RegexExecFlags flags = RegexExecFlags::None)
{
    return regex_search<It, MatchDirection::Backward>(begin, end, subject_begin, subject_end, res, re, flags);
}

String option_to_string(const Regex& re);
Regex option_from_string(Meta::Type<Regex>, StringView str);

template<typename Iterator, MatchDirection direction = MatchDirection::Forward>
struct RegexIterator
{
    using ValueType = MatchResults<Iterator>;
    struct Sentinel{};
    struct It
    {
        It(RegexIterator& base) : m_base(base), m_valid{m_base.next()} {}

        const ValueType& operator*() const { kak_assert(m_valid); return m_base.m_results; }
        const ValueType* operator->() const { kak_assert(m_valid); return &m_base.m_results; }

        It& operator++() { m_valid = m_base.next(); return *this; }

        RegexIterator& m_base;
        bool m_valid;
    };

    friend bool operator==(const It& lhs, Sentinel) { return not lhs.m_valid; }
    friend bool operator!=(const It& lhs, Sentinel) { return lhs.m_valid; }

    RegexIterator(Iterator begin, Iterator end,
                  Iterator subject_begin, Iterator subject_end,
                  const Regex& re, RegexExecFlags flags = RegexExecFlags::None)
        : m_vm{*re.impl()}, m_next_pos{direction == MatchDirection::Forward ? begin : end},
          m_begin{std::move(begin)}, m_end{std::move(end)},
          m_subject_begin{std::move(subject_begin)}, m_subject_end{std::move(subject_end)},
          m_flags{flags} {}

    RegexIterator(const Iterator& begin, const Iterator& end, const Regex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
        : RegexIterator{begin, end, begin, end, re, flags} {}

    It begin() { return {*this}; }
    Sentinel end() const { return {}; }

private:
    bool next()
    {
        auto additional_flags = RegexExecFlags::Search;
        if (m_results.size() and m_results[0].first == m_results[0].second)
            additional_flags |= RegexExecFlags::NotInitialNull;

        constexpr bool forward = direction == MatchDirection::Forward;

        if (not m_vm.exec(forward ? m_next_pos : m_begin, forward ? m_end : m_next_pos,
                          m_subject_begin, m_subject_end, m_flags | additional_flags))
            return false;

        m_results.values().clear();
        std::move(m_vm.captures().begin(), m_vm.captures().end(), std::back_inserter(m_results.values()));
        m_next_pos = (direction == MatchDirection::Forward) ? m_results[0].second : m_results[0].first;
        return true;
    }

    ThreadedRegexVM<Iterator, direction> m_vm;
    MatchResults<Iterator> m_results;
    Iterator m_next_pos{};
    const Iterator m_begin{};
    const Iterator m_end{};
    const Iterator m_subject_begin{};
    const Iterator m_subject_end{};
    const RegexExecFlags m_flags = RegexExecFlags::None;
};

}

#endif // regex_hh_INCLUDED
