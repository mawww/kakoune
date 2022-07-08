#include "ranked_match.hh"

#include "flags.hh"
#include "string_utils.hh"
#include "unit_tests.hh"
#include "utf8_iterator.hh"
#include "optional.hh"
#include "vector.hh"

#include <algorithm>
#include <cmath>
#include <bit>

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

static bool is_word_boundary(Codepoint prev, Codepoint c)
{
    return (iswalnum((wchar_t)prev) != iswalnum((wchar_t)c)) or
           (iswlower((wchar_t)prev) and iswupper((wchar_t)c));
}

static int word_start_factor(Codepoint prev, Codepoint c)
{
    return 2 * (not iswalnum((wchar_t)prev) and iswalnum((wchar_t)c)) +
           1 * (iswlower((wchar_t)prev) and iswupper((wchar_t)c));
}

static bool smartcase_eq(Codepoint candidate_c, Codepoint query_c,
                         const RankedMatchQuery& query, CharCount query_i)
{
    return candidate_c == query_c or Optional{candidate_c} == query.smartcase_alternative_match[(size_t)query_i];
}

static bool greedy_subsequence_match(StringView candidate, const RankedMatchQuery& query)
{
    auto it = candidate.begin();
    CharCount query_i = 0;
    for (auto query_it = query.input.begin(); query_it != query.input.end();)
    {
        if (it == candidate.end())
            return false;
        const Codepoint c = utf8::read_codepoint(query_it, query.input.end());
        while (true)
        {
            auto candidate_c = utf8::read_codepoint(it, candidate.end());
            if (smartcase_eq(candidate_c, c, query, query_i))
                break;

            if (it == candidate.end())
                return false;
        }
        query_i++;
    }
    return true;
}

static CharCount query_length(const RankedMatchQuery& query)
{
    return query.smartcase_alternative_match.size();
}

// Below is an implementation of Gotoh's algorithm for optimal sequence
// alignment following the 1982 paper "An Improved Algorithm for Matching
// Biological Sequences", see
// https://courses.cs.duke.edu/spring21/compsci260/resources/AlignmentPapers/1982.gotoh.pdf
// We simplify the algorithm by always requiring the query to be a subsequence
// of the candidate.
struct Distance
{
    // The distance between two strings.
    int distance = 0;
    // The distance between two strings if the alignment ends in a gap.
    int distance_ending_in_gap = 0;
};

template<bool full_matrix>
class SubsequenceDistance
{
public:
    SubsequenceDistance(const RankedMatchQuery& query, StringView candidate)
        : query{query}, candidate{candidate},
          stride{candidate.char_length() + 1},
          m_matrix{(size_t)(
              (full_matrix ? (query_length(query) + 1) : 2)
              * stride)} {}

    ArrayView<Distance, CharCount> operator[](CharCount query_i)
    {
        return {m_matrix.data() + (size_t)(query_i * stride), stride};
    }
    ConstArrayView<Distance, CharCount> operator[](CharCount query_i) const
    {
        return {m_matrix.data() + (size_t)(query_i * stride), stride};
    }

    // The index of the last matched character.
    CharCount max_index = 0;
    // These fields exist to allow pretty-printing in GDB.
    const RankedMatchQuery& query;
    const StringView candidate;
private:
    CharCount stride;
    // For each combination of prefixes of candidate and query, this holds
    // their distance.  For example, (*this)[2][3] holds the distance between
    // the first two query characters and the first three characters from
    // the candidate.
    Vector<Distance, MemoryDomain::RankedMatch> m_matrix;
};

static constexpr int infinity = std::numeric_limits<int>::max();
constexpr int max_index_weight = 1;

template<bool full_matrix>
static SubsequenceDistance<full_matrix> subsequence_distance(const RankedMatchQuery& query, StringView candidate)
{
    auto match_bonus = [](int word_start, bool is_same_case) -> int {
        return -75 * word_start
               -40
                -4 * is_same_case;
    };
    constexpr int gap_weight = 200;
    constexpr int gap_extend_weight = 1;

    SubsequenceDistance<full_matrix> distance{query, candidate};

    CharCount candidate_length = candidate.char_length();

    // Compute the distance of skipping a prefix of the candidate.
    for (CharCount candidate_i = 0; candidate_i <= candidate_length; candidate_i++)
        distance[0][candidate_i].distance = (int)candidate_i * gap_extend_weight;

    CharCount query_i, candidate_i;
    String::const_iterator query_it, candidate_it;
    for (query_i = 1, query_it = query.input.begin();
         query_i <= query_length(query);
         query_i++)
    {
        CharCount query_virtual_i = query_i;
        CharCount prev_query_virtual_i = query_i - 1;
        // Only keep the last two rows in memory, swapping them in each iteration.
        if constexpr (not full_matrix)
        {
            query_virtual_i %= 2;
            prev_query_virtual_i %= 2;
        }

        auto row = distance[query_virtual_i];
        auto prev_row = distance[prev_query_virtual_i];

        // Since we only allow subsequence matches, we don't need deletions.
        // This rules out prefix-matches where the query is longer than the
        // candidate.  Mark them as impossible. We only need to mark the boundary
        // cases since we never read others.
        if (query_i - 1 <= candidate_length)
        {
            row[query_i - 1].distance = infinity;
            row[query_i - 1].distance_ending_in_gap = infinity;
        }
        Codepoint query_c = utf8::read_codepoint(query_it, query.input.end());
        Codepoint prev_c;
        // Since we don't allow deletions, the candidate must be at least
        // as long as the query. This allows us to skip some cells.
        for (candidate_i = query_i,
                 candidate_it = utf8::advance(candidate.begin(), candidate.end(), query_i -1),
                 prev_c = utf8::prev_codepoint(candidate_it, candidate.begin()).value_or(Codepoint(0));
             candidate_i <= candidate_length;
             candidate_i++)
        {
            Codepoint candidate_c = utf8::read_codepoint(candidate_it, candidate.end());

            int distance_ending_in_gap = infinity;
            if (auto parent = row[candidate_i - 1]; parent.distance != infinity)
            {
                bool is_trailing_gap = query_i == query_length(query);
                int start_gap = parent.distance + (gap_weight * not is_trailing_gap) + gap_extend_weight;
                int extend_gap = parent.distance_ending_in_gap == infinity
                                    ? infinity
                                    : parent.distance_ending_in_gap + gap_extend_weight;
                distance_ending_in_gap = std::min(start_gap, extend_gap);
            }

            int distance_match = infinity;
            if (Distance parent = prev_row[candidate_i - 1];
                parent.distance != infinity and smartcase_eq(candidate_c, query_c, query, query_i - 1))
            {
                int word_start = word_start_factor(prev_c, candidate_c);
                bool is_same_case = candidate_c == query_c;
                distance_match = parent.distance + match_bonus(word_start, is_same_case);
            }

            row[candidate_i].distance = std::min(distance_match, distance_ending_in_gap);
            row[candidate_i].distance_ending_in_gap = distance_ending_in_gap;
            if (query_i == query_length(query) and distance_match < distance_ending_in_gap)
                distance.max_index = candidate_i - 1;
            prev_c = candidate_c;
        }
    }
    return distance;
}

RankedMatchQuery::RankedMatchQuery(StringView query) : RankedMatchQuery(query, {}) {}

RankedMatchQuery::RankedMatchQuery(StringView input, UsedLetters used_letters)
    : input(input), used_letters(used_letters),
      smartcase_alternative_match(input | transform([](Codepoint c) -> Optional<Codepoint> {
          if (is_lower(c))
              return to_upper(c);
          return {};
      }) | gather<decltype(smartcase_alternative_match)>()) {}

template<typename TestFunc>
RankedMatch::RankedMatch(StringView candidate, const RankedMatchQuery& query, TestFunc func)
{
    if (query.input.length() > candidate.length())
        return;

    if (query.input.empty())
    {
        m_candidate = candidate;
        m_matches = true;
        return;
    }

    if (not func())
        return;

    // Our matching is quadratic; avoid a hypothetical blowup by only looking at a prefix.
    constexpr CharCount candidate_max_length = 1000;
    StringView bounded_candidate = candidate.char_length() > candidate_max_length
                                 ? candidate.substr(0, candidate_max_length)
                                 : candidate;

    if (not greedy_subsequence_match(bounded_candidate, query))
        return;

    m_candidate = candidate;
    m_matches = true;

    auto distance = subsequence_distance<false>(query, bounded_candidate);

    m_distance = distance[query_length(query) % 2][bounded_candidate.char_length()].distance
               + (int)distance.max_index * max_index_weight;
}

RankedMatch::RankedMatch(StringView candidate, UsedLetters candidate_letters,
                         const RankedMatchQuery& query)
    : RankedMatch{candidate, query, [&] {
        return matches(to_lower(query.used_letters), to_lower(candidate_letters)) and
               matches(query.used_letters & upper_mask, candidate_letters & upper_mask);
    }} {}


RankedMatch::RankedMatch(StringView candidate, const RankedMatchQuery& query)
    : RankedMatch{candidate, query, [] { return true; }}
{
}

bool RankedMatch::operator<(const RankedMatch& other) const
{
    kak_assert((bool)*this and (bool)other);

    if (m_distance != other.m_distance)
        return m_distance < other.m_distance;

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

            const bool low1 = iswlower((wchar_t)cp1);
            const bool low2 = iswlower((wchar_t)cp2);
            if (low1 != low2)
                return low1;

            return order(cp1) < order(cp2);
        }
        last1 = it1; last2 = it2;
    }
}

// returns the base-2 logarithm, rounded down
constexpr uint32_t log2(uint32_t n) noexcept
{
    return 31 - std::countl_zero(n);
}

static_assert(log2(1) == 0);
static_assert(log2(2) == 1);
static_assert(log2(3) == 1);
static_assert(log2(4) == 2);

// returns a string representation of the distance matrix, for debugging only
[[maybe_unused]] static String to_string(const SubsequenceDistance<true>& distance)
{
    const RankedMatchQuery& query = distance.query;
    StringView candidate = distance.candidate;

    auto candidate_length = candidate.char_length();

    int distance_amplitude = 1;
    for (auto query_i = 0; query_i <= query_length(query); query_i++)
        for (auto candidate_i = 1; candidate_i <= candidate_length; candidate_i++)
            if (distance[query_i][candidate_i].distance != infinity)
                distance_amplitude = std::max(distance_amplitude, std::abs(distance[query_i][candidate_i].distance));
    ColumnCount max_digits = log2(distance_amplitude) / log2(10) + 1;
    ColumnCount cell_width = 2 * (max_digits + 1) // two numbers with a minus sign
                           + 2; // separator between the numbers, plus one space

    String s = "\n";
    auto query_it = query.input.begin();
    s += String{' ', cell_width};
    for (auto query_i = 0; query_i <= query_length(query); query_i++)
    {
        Codepoint query_c = query_i == 0 ? ' ' : utf8::read_codepoint(query_it, query.input.end());
        s += left_pad(to_string(query_c), cell_width);
    }
    s += "\n";

    auto candidate_it = candidate.begin();
    for (CharCount candidate_i = 0; candidate_i <= candidate_length; candidate_i++)
    {
        Codepoint candidate_c = candidate_i == 0 ? ' ' : utf8::read_codepoint(candidate_it, candidate.end());
        s += left_pad(to_string(candidate_c), cell_width);
        for (CharCount query_i = 0; query_i <= query_length(query); query_i++)
        {
            auto distance_to_string = [](int d) -> String {
                return d == infinity ? String{"∞"} : to_string(d);
            };
            Distance cell = distance[query_i][candidate_i];
            s += left_pad(
                distance_to_string(cell.distance) + "/" + distance_to_string(cell.distance_ending_in_gap),
                cell_width);
        }
        s += "\n";
    }

    return s;
}

UnitTest test_ranked_match{[] {
    // Convenience variables, for debugging only.
    Optional<RankedMatchQuery> q;
    Optional<SubsequenceDistance<true>> distance_better;
    Optional<SubsequenceDistance<true>> distance_worse;

    auto preferred = [&](StringView query, StringView better, StringView worse) -> bool {
        q = RankedMatchQuery{query};
        distance_better = subsequence_distance<true>(*q, better);
        distance_worse = subsequence_distance<true>(*q, worse);
        return RankedMatch{better, *q} < RankedMatch{worse, *q};
    };

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
    kak_assert(preferred("gre", "*grep*", ".git/rebase-merge/git-rebase-todo"));
    kak_assert(preferred("CAPRAN", "CAPABILITY_RANGE_FORMATTING", "CAPABILITY_SELECTION_RANGE"));
    kak_assert(preferred("mal", "malt", "formal"));
    kak_assert(preferred("fa", "face", "find-apply-changes"));
    kak_assert(preferred("cne", "cargo-next-error", "comment-line"));
    kak_assert(preferred("cne", "cargo-next-error", "ccls-navigate"));
    kak_assert(preferred("cpe", "cargo-previous-error", "cpp-alternative-file"));
    kak_assert(preferred("server_",  "server_capabilities", "SERVER_CANCELLED"));
    kak_assert(preferred("server_",  "server_capabilities_capabilities", "SERVER_CANCELLED"));
    kak_assert(preferred("codegen", "clang/test/CodeGen/asm.c", "clang/test/ASTMerge/codegen-body/test.c"));
    kak_assert(preferred("cho", "tchou kanaky", "tachou kanay")); // Prefer the leftmost match.
    kak_assert(preferred("clang-query", "clang/tools/clang-query/ClangQuery.cpp", "clang/test/Tooling/clang-query.cpp"));
    kak_assert(preferred("clangd", "clang-tools-extra/clangd/README.md", "clang/docs/conf.py"));
    kak_assert(preferred("rm.cc", "ranked_match.cc", "remote.cc"));
    kak_assert(preferred("rm.cc", "src/ranked_match.cc", "test/README.asciidoc"));
    kak_assert(preferred("fooo", "foo.o", "fo.o.o"));
    kak_assert(preferred("evilcorp-lint/bar.go", "scripts/evilcorp-lint/foo/bar.go", "src/evilcorp-client/foo/bar.go"));
    kak_assert(preferred("lang/haystack/needle.c", "git.evilcorp.com/language/haystack/aaa/needle.c", "git.evilcorp.com/aaa/ng/wrong-haystack/needle.cpp"));
    kak_assert(preferred("luaremote", "src/script/LuaRemote.cpp", "tests/TestLuaRemote.cpp"));
}};

UnitTest test_used_letters{[]()
{
    kak_assert(used_letters("abcd") == to_lower(used_letters("abcdABCD")));
}};

}
