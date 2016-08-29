#ifndef ranked_match_hh_INCLUDED
#define ranked_match_hh_INCLUDED

#include "string.hh"
#include "flags.hh"

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

    enum class Flags : int
    {
        None = 0,
        // Order is important, the highest bit has precedence for comparison
        OnlyWordBoundary = 1 << 0,
        FirstCharMatch = 1 << 1,
        Prefix = 1 << 2,
        FullMatch = 1 << 3,
    };

    StringView m_candidate;
    Flags m_flags = Flags::None;
    int m_word_boundary_match_count = 0;
    int m_max_index = 0;
};

template<> struct WithBitOps<RankedMatch::Flags> : std::true_type {};

}

#endif // ranked_match_hh_INCLUDED
