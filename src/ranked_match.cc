#include "ranked_match.hh"

#include "unit_tests.hh"

namespace Kakoune
{

static int count_word_boundaries_match(StringView candidate, StringView query)
{
    int count = 0;
    auto it = query.begin();
    char prev = 0;
    for (auto c : candidate)
    {
        const bool is_word_boundary = prev == 0 or
                                      (ispunct(prev) and is_word(c)) or
                                      (islower(prev) and isupper(c));
        prev = c;

        if (not is_word_boundary)
            continue;

        const char lc = tolower(c);
        for (; it != query.end(); ++it)
        {
            const char qc = *it;
            if (qc == (islower(qc) ? lc  : c))
            {
                ++count;
                ++it;
                break;
            }
        }
        if (it == query.end())
            break;
    }
    return count;
}

static bool smartcase_eq(char query, char candidate)
{
    return query == (islower(query) ? tolower(candidate) : candidate);
}

static bool subsequence_match_smart_case(StringView str, StringView subseq)
{
    auto it = str.begin();
    for (auto& c : subseq)
    {
        if (it == str.end())
            return false;
        while (not smartcase_eq(c, *it))
        {
            if (++it == str.end())
                return false;
        }
        ++it;
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

    if (not subsequence_match_smart_case(candidate, query))
        return;

    m_candidate = candidate;

    m_first_char_match = smartcase_eq(query[0], candidate[0]);
    m_word_boundary_match_count = count_word_boundaries_match(candidate, query);
    m_only_word_boundary = m_word_boundary_match_count == query.length();
    m_prefix = std::equal(query.begin(), query.end(), candidate.begin(), smartcase_eq);
}

bool RankedMatch::operator<(const RankedMatch& other) const
{
    if (m_only_word_boundary or other.m_only_word_boundary)
        return m_only_word_boundary and other.m_only_word_boundary ?
            m_word_boundary_match_count > other.m_word_boundary_match_count
          : m_only_word_boundary;

    if (m_prefix != other.m_prefix)
        return m_prefix;

    if (m_word_boundary_match_count != other.m_word_boundary_match_count)
        return m_word_boundary_match_count > other.m_word_boundary_match_count;

    if (m_first_char_match != other.m_first_char_match)
        return m_first_char_match;

    return std::lexicographical_compare(
        m_candidate.begin(), m_candidate.end(),
        other.m_candidate.begin(), other.m_candidate.end(),
        [](char a, char b) {
            const bool low_a = islower(a), low_b = islower(b);
            return low_a == low_b ? a < b : low_a;
        });
}

UnitTest test_ranked_match{[] {
    kak_assert(count_word_boundaries_match("run_all_tests", "rat") == 3);
}};

}
