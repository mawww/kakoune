#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "flags.hh"
#include "selection.hh"

namespace Kakoune
{

class Regex;
template<typename Iterator> struct MatchResults;

inline Selection keep_direction(Selection res, const Selection& ref)
{
    if ((res.cursor() < res.anchor()) != (ref.cursor() < ref.anchor()))
        std::swap<BufferCoord>(res.cursor(), res.anchor());
    return res;
}

template<WordType word_type>
Optional<Selection>
select_to_next_word(const Buffer& buffer, const Selection& selection);

template<WordType word_type>
Optional<Selection>
select_to_next_word_end(const Buffer& buffer, const Selection& selection);

template<WordType word_type>
Optional<Selection>
select_to_previous_word(const Buffer& buffer, const Selection& selection);

Optional<Selection>
select_line(const Buffer& buffer, const Selection& selection);

Optional<Selection>
select_matching(const Buffer& buffer, const Selection& selection);

Optional<Selection>
select_to(const Buffer& buffer, const Selection& selection,
                    Codepoint c, int count, bool inclusive);
Optional<Selection>
select_to_reverse(const Buffer& buffer, const Selection& selection,
                  Codepoint c, int count, bool inclusive);

template<bool only_move>
Optional<Selection>
select_to_line_end(const Buffer& buffer, const Selection& selection);

template<bool only_move>
Optional<Selection>
select_to_line_begin(const Buffer& buffer, const Selection& selection);

Optional<Selection>
select_to_first_non_blank(const Buffer& buffer, const Selection& selection);

enum class ObjectFlags
{
    ToBegin = 1,
    ToEnd   = 2,
    Inner   = 4
};

template<> struct WithBitOps<ObjectFlags> : std::true_type {};

template<WordType word_type>
Optional<Selection>
select_word(const Buffer& buffer, const Selection& selection,
            int count, ObjectFlags flags);

Optional<Selection>
select_number(const Buffer& buffer, const Selection& selection,
              int count, ObjectFlags flags);

Optional<Selection>
select_sentence(const Buffer& buffer, const Selection& selection,
                int count, ObjectFlags flags);

Optional<Selection>
select_paragraph(const Buffer& buffer, const Selection& selection,
                 int count, ObjectFlags flags);

Optional<Selection>
select_whitespaces(const Buffer& buffer, const Selection& selection,
                   int count, ObjectFlags flags);

Optional<Selection>
select_indent(const Buffer& buffer, const Selection& selection,
              int count, ObjectFlags flags);

Optional<Selection>
select_argument(const Buffer& buffer, const Selection& selection,
                int level, ObjectFlags flags);

Optional<Selection>
select_lines(const Buffer& buffer, const Selection& selection);

Optional<Selection>
trim_partial_lines(const Buffer& buffer, const Selection& selection);

void select_buffer(SelectionList& selections);

enum Direction { Forward, Backward };

template<Direction direction>
Selection find_next_match(const Buffer& buffer, const Selection& sel,
                          const Regex& regex, bool& wrapped);

void select_all_matches(SelectionList& selections, const Regex& regex, int capture = 0);
void split_selections(SelectionList& selections, const Regex& separator_regex, int capture = 0);

Optional<Selection>
select_surrounding(const Buffer& buffer, const Selection& selection,
                             StringView opening, StringView closing, int level,
                             ObjectFlags flags);

}

#endif // selectors_hh_INCLUDED
