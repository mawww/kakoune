#include "ranked_match.hh"

namespace Kakoune
{

int match_rank(StringView candidate, StringView query)
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

}
