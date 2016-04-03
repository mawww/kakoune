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
                                      (!iswalnum(prev) and iswalnum(c)) or
                                      (islower(prev) and isupper(c));
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

static bool subsequence_match_smart_case(StringView str, StringView subseq, int& index_sum)
{
    index_sum = 0;
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
        index_sum += index++;
    }
    return true;
}

template<typename TestFunc>
RankedMatch::RankedMatch(StringView candidate, StringView query, TestFunc func)
{
    if (candidate.empty() or query.length() > candidate.length())
        return;

    if (query.empty())
        m_candidate = candidate;
    else if (func() and  subsequence_match_smart_case(candidate, query, m_match_index_sum))
    {
        m_candidate = candidate;

        m_first_char_match = smartcase_eq(query[0], candidate[0]);
        m_word_boundary_match_count = count_word_boundaries_match(candidate, query);
        m_only_word_boundary = m_word_boundary_match_count == query.length();
        m_prefix = std::equal(query.begin(), query.end(), candidate.begin());
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

    if (m_prefix != other.m_prefix)
        return m_prefix;

    if (m_first_char_match != other.m_first_char_match)
        return m_first_char_match;

    if (m_only_word_boundary and other.m_only_word_boundary)
    {
        if (m_word_boundary_match_count != other.m_word_boundary_match_count)
            return m_word_boundary_match_count > other.m_word_boundary_match_count;
    }
    else if (m_only_word_boundary or other.m_only_word_boundary)
        return  m_only_word_boundary;

    if (m_word_boundary_match_count != other.m_word_boundary_match_count)
        return m_word_boundary_match_count > other.m_word_boundary_match_count;

    if (m_match_index_sum != other.m_match_index_sum)
        return m_match_index_sum < other.m_match_index_sum;

    for (Utf8It it1{m_candidate.begin(), m_candidate}, it2{other.m_candidate.begin(), other.m_candidate};
         it1 != m_candidate.end() and it2 != other.m_candidate.end(); ++it1, ++it2)
    {
        const auto cp1 = *it1, cp2 = *it2;
        if (cp1 != cp2)
        {
            const bool low1 = islower(cp1), low2 = islower(cp2);
            return low1 == low2 ? cp1 < cp2 : low1;
        }
    }

    return false;
}

UnitTest test_ranked_match{[] {
    kak_assert(count_word_boundaries_match("run_all_tests", "rat") == 3);
    kak_assert(count_word_boundaries_match("run_all_tests", "at") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "wm") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cobm") == 3);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cWBM") == 4);
}};

UnitTest test_used_letters{[]()
{
    kak_assert(used_letters("abcd") == to_lower(used_letters("abcdABCD")));
}};

}
