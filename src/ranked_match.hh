#ifndef ranked_match_hh_INCLUDED
#define ranked_match_hh_INCLUDED

#include "string.hh"
#include "meta.hh"

#include <cstdint>

namespace Kakoune
{

using UsedLetters = uint64_t;
UsedLetters used_letters(StringView str);

constexpr UsedLetters upper_mask = 0xFFFFFFC000000;

inline UsedLetters to_lower(UsedLetters letters)
{
    return ((letters & upper_mask) >> 26) | (letters & (~upper_mask));
}

struct RankedMatch
{
    RankedMatch(StringView candidate, StringView query);
    RankedMatch(StringView candidate, UsedLetters candidate_letters,
                StringView query, UsedLetters query_letters);

    const StringView& candidate() const { return m_candidate; }
    bool operator<(const RankedMatch& other) const;
    bool operator==(const RankedMatch& other) const { return m_candidate == other.m_candidate; }

    explicit operator bool() const { return m_matches; }

private:
    template<typename TestFunc>
    RankedMatch(StringView candidate, StringView query, TestFunc test);

    StringView m_candidate{};
    bool m_matches = false;
    int m_distance = 0;
};

}

#endif // ranked_match_hh_INCLUDED
