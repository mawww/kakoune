#ifndef ranked_match_hh_INCLUDED
#define ranked_match_hh_INCLUDED

#include "string.hh"

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

    explicit operator bool() const { return not m_candidate.empty(); }

private:
    template<typename TestFunc>
    RankedMatch(StringView candidate, StringView query, TestFunc test);

    StringView m_candidate;
    bool m_first_char_match = false;
    bool m_prefix = false;
    int m_word_boundary_match_count = 0;
    int m_match_index_sum = 0;
    bool m_only_word_boundary = false;
};

}

#endif // ranked_match_hh_INCLUDED
