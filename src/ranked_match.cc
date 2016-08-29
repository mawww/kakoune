#include "ranked_match.hh"

#include "utf8_iterator.hh"
#include "unit_tests.hh"

namespace Kakoune
{

UsedLetters used_letters(StringView str)
{
    UsedLetters res = 0;
    for (auto c : str)
    {
        if (c >= 'a' and c <= 'z')
            res |= 1uL << (c - 'a');
        else if (c >= 'A' and c <= 'Z')
            res |= 1uL << (c - 'A' + 26);
        else if (c == '_')
            res |= 1uL << 53;
        else if (c == '-')
            res |= 1uL << 54;
        else
            res |= 1uL << 63;
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
                                      (!iswalnum((wchar_t)prev) and iswalnum((wchar_t)c)) or
                                      (iswlower((wchar_t)prev) and iswupper((wchar_t)c));
        prev = c;

        if (not is_word_boundary)
            continue;

        const Codepoint lc = to_lower(c);
        for (auto qit = query_it; qit != query.end(); ++qit)
        {
            const Codepoint qc = *qit;
            if (qc == (islower(qc) ? lc  : c))
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

static bool smartcase_eq(Codepoint query, Codepoint candidate)
{
    return query == (islower(query) ? to_lower(candidate) : candidate);
}

static bool subsequence_match_smart_case(StringView str, StringView subseq, int& out_max_index)
{
    int max_index = 0;
    auto it = str.begin();
    int index = 0;
    for (auto subseq_it = subseq.begin(); subseq_it != subseq.end();)
    {
        if (it == str.end())
            return false;
        const Codepoint c = utf8::read_codepoint(subseq_it, subseq.end());
        while (not smartcase_eq(c, utf8::read_codepoint(it, subseq.end())))
        {
            ++index;
            if (it == str.end())
                return false;
        }
        max_index = index++;
    }
    out_max_index = max_index;
    return true;
}

template<typename TestFunc>
RankedMatch::RankedMatch(StringView candidate, StringView query, TestFunc func)
{
    if (candidate.empty() or query.length() > candidate.length())
        return;

    if (query.empty())
        m_candidate = candidate;
    else if (func() and  subsequence_match_smart_case(candidate, query, m_max_index))
    {
        m_candidate = candidate;

        if (smartcase_eq(query[0], candidate[0]))
            m_flags |= Flags::FirstCharMatch;
        if (std::equal(query.begin(), query.end(), candidate.begin()))
        {
            m_flags |= Flags::Prefix;
            if (query.length() == candidate.length())
                m_flags |= Flags::FullMatch;
        }
        m_word_boundary_match_count = count_word_boundaries_match(candidate, query);
        if (m_word_boundary_match_count == query.length())
            m_flags |= Flags::OnlyWordBoundary;
    }
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

bool RankedMatch::operator<(const RankedMatch& other) const
{
    kak_assert((bool)*this and (bool)other);

    const auto diff = m_flags ^ other.m_flags;
    // flags are different, use their ordering to return the first match
    if (diff != Flags::None)
        return (int)(m_flags & diff) > (int)(other.m_flags & diff);

    if (m_word_boundary_match_count != other.m_word_boundary_match_count)
        return m_word_boundary_match_count > other.m_word_boundary_match_count;

    if (m_max_index != other.m_max_index)
        return m_max_index < other.m_max_index;

    auto it1 = m_candidate.begin(), it2 = other.m_candidate.begin();
    const auto end1 = m_candidate.end(), end2 = other.m_candidate.end();
    while (true)
    {
        // find next mismatch
        while (it1 != end1 and it2 != end2 and *it1 == *it2)
            ++it1, ++it2;

        if (it1 == end1 or it2 == end2)
            return it1 == end1 and it2 != end2;

        // compare codepoints
        it1 = utf8::character_start(it1, m_candidate.begin());
        it2 = utf8::character_start(it2, other.m_candidate.begin());
        const auto cp1 = utf8::read_codepoint(it1, end1);
        const auto cp2 = utf8::read_codepoint(it2, end2);;
        if (cp1 != cp2)
        {
            const bool low1 = islower(cp1), low2 = islower(cp2);
            return low1 == low2 ? cp1 < cp2 : low1;
        }
    }
}

UnitTest test_ranked_match{[] {
    kak_assert(count_word_boundaries_match("run_all_tests", "rat") == 3);
    kak_assert(count_word_boundaries_match("run_all_tests", "at") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "wm") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cobm") == 3);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cWBM") == 4);
    kak_assert(RankedMatch{"source", "so"} < RankedMatch{"source_data", "so"});
    kak_assert(not (RankedMatch{"source_data", "so"} < RankedMatch{"source", "so"}));
    kak_assert(not (RankedMatch{"source", "so"} < RankedMatch{"source", "so"}));
}};

UnitTest test_used_letters{[]()
{
    kak_assert(used_letters("abcd") == to_lower(used_letters("abcdABCD")));
}};

}
