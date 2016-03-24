#include "ranked_match.hh"

#include "utf8_iterator.hh"
#include "unit_tests.hh"

namespace Kakoune
{

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
    for (auto subseq_it = subseq.begin(); subseq_it != subseq.end();
         subseq_it = utf8::next(subseq_it, subseq.end()))
    {
        if (it == str.end())
            return false;
        const Codepoint c = utf8::codepoint(subseq_it, subseq.end());
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

RankedMatch::RankedMatch(StringView candidate, StringView query)
{
    if (candidate.empty() or query.length() > candidate.length())
        return;

    if (query.empty())
    {
        m_candidate = candidate;
        return;
    }

    if (not subsequence_match_smart_case(candidate, query, m_match_index_sum))
        return;

    m_candidate = candidate;

    m_first_char_match = smartcase_eq(query[0], candidate[0]);
    m_word_boundary_match_count = count_word_boundaries_match(candidate, query);
    m_only_word_boundary = m_word_boundary_match_count == query.length();
    m_prefix = std::equal(query.begin(), query.end(), candidate.begin(), smartcase_eq);
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

    return std::lexicographical_compare(
        Utf8It{m_candidate.begin(), m_candidate}, Utf8It{m_candidate.end(), m_candidate},
        Utf8It{other.m_candidate.begin(), other.m_candidate}, Utf8It{other.m_candidate.end(), other.m_candidate},
        [](Codepoint a, Codepoint b) {
            const bool low_a = islower(a), low_b = islower(b);
            return low_a == low_b ? a < b : low_a;
        });
}

UnitTest test_ranked_match{[] {
    kak_assert(count_word_boundaries_match("run_all_tests", "rat") == 3);
    kak_assert(count_word_boundaries_match("run_all_tests", "at") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "wm") == 2);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cobm") == 3);
    kak_assert(count_word_boundaries_match("countWordBoundariesMatch", "cWBM") == 4);
}};

}
