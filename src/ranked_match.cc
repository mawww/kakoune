#include "ranked_match.hh"

#include "flags.hh"
#include "unit_tests.hh"
#include "utf8_iterator.hh"
#include "optional.hh"
#include "ranges.hh"

#include <algorithm>

namespace Kakoune
{

UsedLetters used_letters(StringView str)
{
    UsedLetters res = 0;
    for (auto c : str)
    {
        if (c >= 'a' and c <= 'z')
            res |= 1uLL << (c - 'a');
        else if (c >= 'A' and c <= 'Z')
            res |= 1uLL << (c - 'A' + 26);
        else if (c == '_')
            res |= 1uLL << 53;
        else if (c == '-')
            res |= 1uLL << 54;
        else
            res |= 1uLL << 63;
    }
    return res;
}

bool matches(UsedLetters query, UsedLetters letters)
{
    return (query & letters) == query;
}

using Utf8It = utf8::iterator<const char*>;

static int count_word_boundaries_match(StringView candidate, StringView query)
{
    int count = 0;
    Utf8It query_it{query.begin(), query};
    Codepoint prev = 0;
    for (Utf8It it{candidate.begin(), candidate}; it != candidate.end(); ++it)
    {
        const Codepoint c = *it;
        const bool is_word_boundary = prev == 0 or
                                      (!is_word(prev, {}) and is_word(c, {})) or
                                      (is_lower(prev) and is_upper(c));
        prev = c;

        if (not is_word_boundary)
            continue;

        const Codepoint lc = to_lower(c);
        for (auto qit = query_it; qit != query.end(); ++qit)
        {
            const Codepoint qc = *qit;
            if (qc == (is_lower(qc) ? lc  : c))
            {
                ++count;
                query_it = qit+1;
                break;
            }
        }
        if (query_it == query.end())
            break;
    }
    return count;
}

static bool smartcase_eq(Codepoint candidate, Codepoint query)
{
    return query == (is_lower(query) ? to_lower(candidate) : candidate);
}

struct SubseqRes
{
    int max_index;
    bool single_word;
};

static Optional<SubseqRes> subsequence_match_smart_case(StringView str, StringView subseq)
{
    bool single_word = true;
    int max_index = -1;
    auto it = str.begin();
    int index = 0;
    for (auto subseq_it = subseq.begin(); subseq_it != subseq.end();)
    {
        if (it == str.end())
            return {};
        const Codepoint c = utf8::read_codepoint(subseq_it, subseq.end());
        if (single_word and not is_word(c))
            single_word = false;
        while (true)
        {
            auto str_c = utf8::read_codepoint(it, str.end());
            if (smartcase_eq(str_c, c))
                break;

            if (max_index != -1 and single_word and not is_word(str_c))
                single_word = false;

            ++index;
            if (it == str.end())
                return {};
        }
        max_index = index++;
    }
    return SubseqRes{max_index, single_word};
}

template<typename TestFunc>
RankedMatch::RankedMatch(StringView candidate, StringView query, TestFunc func)
{
    if (query.length() > candidate.length())
        return;

    if (query.empty())
    {
        m_candidate = candidate;
        m_matches = true;
        return;
    }

    if (not func())
        return;

    auto res = subsequence_match_smart_case(candidate, query);
    if (not res)
        return;

    m_candidate = candidate;
    m_matches = true;
    m_max_index = res->max_index;

    if (res->single_word)
        m_flags |= Flags::SingleWord;

    if (auto it = find(candidate | reverse() | skip(1), '/').base();
        it == candidate.begin() or subsequence_match_smart_case({it, candidate.end()}, query))
    {
        m_flags |= Flags::BaseName;
        if ((candidate.end() - it) >= query.length() and
            std::equal(Utf8It{query.begin(), query}, Utf8It{query.end(), query}, Utf8It{it, candidate},
                       [](Codepoint query, Codepoint candidate) { return smartcase_eq(candidate, query); }))
            m_flags |= Flags::Prefix;
    }

    auto it = std::search(candidate.begin(), candidate.end(),
                          query.begin(), query.end(), smartcase_eq);
    if (it != candidate.end())
    {
        m_flags |= Flags::Contiguous;
        if (std::all_of(Utf8It{query.begin(), query}, Utf8It{query.end(), query}, [](auto cp) { return is_word(cp); }))
            m_flags |= Flags::SingleWord;
        if (it == candidate.begin())
        {
            m_flags |= Flags::Prefix;
            if (query.length() == candidate.length())
            {
                m_flags |= Flags::SmartFullMatch;
                if (candidate == query)
                    m_flags |= Flags::FullMatch;
            }
        }
    }

    m_word_boundary_match_count = count_word_boundaries_match(candidate, query);
    if (m_word_boundary_match_count == query.length())
        m_flags |= Flags::OnlyWordBoundary;
}

RankedMatch::RankedMatch(StringView candidate, UsedLetters candidate_letters,
                         StringView query, UsedLetters query_letters)
    : RankedMatch{candidate, query, [&] {
        return matches(to_lower(query_letters), to_lower(candidate_letters)) and
               matches(query_letters & upper_mask, candidate_letters & upper_mask);
    }} {}


RankedMatch::RankedMatch(StringView candidate, StringView query)
    : RankedMatch{candidate, query, [] { return true; }}
{
}

static bool is_word_boundary(Codepoint prev, Codepoint c)
{
    return (is_word(prev, {})) != is_word(c, {}) or
           (is_lower(prev) != is_lower(c));
}

bool RankedMatch::operator<(const RankedMatch& other) const
{
    kak_assert((bool)*this and (bool)other);

    const auto diff = m_flags ^ other.m_flags;
    // flags are different, use their ordering to return the first match
    if (diff != Flags::None)
        return (int)(m_flags & diff) > (int)(other.m_flags & diff);

    // If we are SingleWord, we dont want to take word boundaries from other
    // words into account.
    if (not (m_flags & (Flags::Prefix | Flags::SingleWord)) and
        m_word_boundary_match_count != other.m_word_boundary_match_count)
        return m_word_boundary_match_count > other.m_word_boundary_match_count;

    if (m_max_index != other.m_max_index)
        return m_max_index < other.m_max_index;

    if (m_input_sequence_number != other.m_input_sequence_number)
        return m_input_sequence_number < other.m_input_sequence_number;

    // Reorder codepoints to improve matching behaviour
    auto order = [](Codepoint cp) { return cp == '/' ? 0 : cp; };

    auto it1 = m_candidate.begin(), it2 = other.m_candidate.begin();
    const auto begin1 = it1, begin2 = it2;
    const auto end1 = m_candidate.end(), end2 = other.m_candidate.end();
    auto last1 = it1, last2 = it2;
    while (true)
    {
        // find next mismatch
        while (it1 != end1 and it2 != end2 and *it1 == *it2)
            ++it1, ++it2;

        if (it1 == end1 or it2 == end2)
            return it1 == end1 and it2 != end2;

        // compare codepoints
        it1 = utf8::character_start(it1, last1);
        it2 = utf8::character_start(it2, last2);
        const auto itsave1 = it1, itsave2 = it2;
        const auto cp1 = utf8::read_codepoint(it1, end1);
        const auto cp2 = utf8::read_codepoint(it2, end2);
        if (cp1 != cp2)
        {
            const auto cplast1 = utf8::prev_codepoint(itsave1, begin1).value_or(Codepoint{0});
            const auto cplast2 = utf8::prev_codepoint(itsave2, begin2).value_or(Codepoint{0});
            const bool is_wb1 = is_word_boundary(cplast1, cp1);
            const bool is_wb2 = is_word_boundary(cplast2, cp2);
            if (is_wb1 != is_wb2)
                return is_wb1;

            const bool low1 = is_lower(cp1);
            const bool low2 = is_lower(cp2);
            if (low1 != low2)
                return low1;

            return order(cp1) < order(cp2);
        }
        last1 = it1; last2 = it2;
    }
}

UnitTest test_ranked_match{[] {
    auto preferred = [](StringView query, StringView better, StringView worse) {
        return RankedMatch{better, query} < RankedMatch{worse, query};
    };

    kak_assert(count_word_boundaries_match("run_all_tests", "rat") == 3);
    kak_assert(count_word_boundaries_match("run_all_tests", "at") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "wm") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cobm") == 3);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cWBM") == 4);
    kak_assert(preferred("so", "source", "source_data"));
    kak_assert(not preferred("so", "source_data", "source"));
    kak_assert(not preferred("so", "source", "source"));
    kak_assert(preferred("wo", "single/word", "multiw/ord"));
    kak_assert(preferred("foobar", "foo/bar/foobar", "foo/bar/baz"));
    kak_assert(preferred("db", "delete-buffer", "debug"));
    kak_assert(preferred("ct", "create_task", "constructor"));
    kak_assert(preferred("cla", "class", "class::attr"));
    kak_assert(preferred("meta", "meta/", "meta-a/"));
    kak_assert(preferred("find", "find(1p)", "findfs(8)"));
    kak_assert(preferred("fin", "find(1p)", "findfs(8)"));
    kak_assert(preferred("sys_find", "sys_find(1p)", "sys_findfs(8)"));
    kak_assert(preferred("", "init", "__init__"));
    kak_assert(preferred("ini", "init", "__init__"));
    kak_assert(preferred("", "a", "b"));
    kak_assert(preferred("expresins", "expresions", "expressionism's"));
    kak_assert(preferred("foo_b", "foo/bar/foo_bar.baz", "test/test_foo_bar.baz"));
    kak_assert(preferred("foo_b", "bar/bar_qux/foo_bar.baz", "foo/test_foo_bar.baz"));
    kak_assert(preferred("foo_bar", "bar/foo_bar.baz", "foo_bar/qux.baz"));
    kak_assert(preferred("fb", "foo_bar/", "foo.bar"));
    kak_assert(preferred("foo_bar", "test_foo_bar", "foo_test_bar"));
    kak_assert(preferred("rm.cc", "src/ranked_match.cc", "test/README.asciidoc"));
    kak_assert(preferred("luaremote", "src/script/LuaRemote.cpp", "tests/TestLuaRemote.cpp"));
}};

UnitTest test_used_letters{[]()
{
    kak_assert(used_letters("abcd") == to_lower(used_letters("abcdABCD")));
}};

}
