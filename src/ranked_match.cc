#include "ranked_match.hh"

namespace Kakoune
{

static bool match_rank(StringView candidate, StringView query)
{
    int rank = 0;
    auto it = candidate.begin();
    char prev = 0;
    for (auto c : query)
    {
        if (it == candidate.end())
            return 0;

        const bool islow = islower(c);
        auto eq_c = [islow, c](char ch) { return islow ? tolower(ch) == c : ch == c; };

        if (eq_c(*it)) // improve rank on contiguous
            ++rank;

        while (!eq_c(*it))
        {
            prev = *it;
            if (++it == candidate.end())
                return 0;
        }
        // Improve rank on word boundaries
        if (prev == 0 or prev == '_' or
            (islower(prev) and isupper(*it)))
            rank += 5;

        prev = c;
        ++rank;
        ++it;
    }
    return rank;
}

RankedMatch::RankedMatch(StringView candidate, StringView query)
{
    if (candidate.empty() or query.empty())
    {
        m_candidate = candidate;
        return;
    }

    m_match_rank = match_rank(candidate, query);
}

bool RankedMatch::operator<(const RankedMatch& other) const
{
    if (m_match_rank == other.m_match_rank)
       return std::lexicographical_compare(
           m_candidate.begin(), m_candidate.end(),
           other.m_candidate.begin(), other.m_candidate.end(),
           [](char a, char b) {
               const bool low_a = islower(a), low_b = islower(b);
               return low_a == low_b ? a < b : low_a;
           });

    return m_match_rank < other.m_match_rank;
}

}
