#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "enum.hh"
#include "optional.hh"
#include "meta.hh"
#include "unicode.hh"
#include "vector.hh"

namespace Kakoune
{

struct Selection;
class Buffer;
class Regex;
class Context;

Selection keep_direction(Selection res, const Selection& ref);

template<WordType word_type>
Optional<Selection>
select_to_next_word(const Context& context, const Selection& selection);

template<WordType word_type>
Optional<Selection>
select_to_next_word_end(const Context& context, const Selection& selection);

template<WordType word_type>
Optional<Selection>
select_to_previous_word(const Context& context, const Selection& selection);

Optional<Selection>
select_line(const Context& context, const Selection& selection);

template<bool forward>
Optional<Selection>
select_matching(const Context& context, const Selection& selection);

Optional<Selection>
select_to(const Context& context, const Selection& selection,
                    Codepoint c, int count, bool inclusive);
Optional<Selection>
select_to_reverse(const Context& context, const Selection& selection,
                  Codepoint c, int count, bool inclusive);

template<bool only_move>
Optional<Selection>
select_to_line_end(const Context& context, const Selection& selection);

template<bool only_move>
Optional<Selection>
select_to_line_begin(const Context& context, const Selection& selection);

Optional<Selection>
select_to_first_non_blank(const Context& context, const Selection& selection);

enum class ObjectFlags
{
    ToBegin = 1,
    ToEnd   = 2,
    Inner   = 4
};

constexpr bool with_bit_ops(Meta::Type<ObjectFlags>) { return true; }

constexpr auto enum_desc(Meta::Type<ObjectFlags>)
{
    return make_array<EnumDesc<ObjectFlags>>({
        { ObjectFlags::ToBegin, "to_begin" },
        { ObjectFlags::ToEnd, "to_end" },
        { ObjectFlags::Inner, "inner" },
    });
}

template<WordType word_type>
Optional<Selection>
select_word(const Context& context, const Selection& selection,
            int count, ObjectFlags flags);

Optional<Selection>
select_number(const Context& context, const Selection& selection,
              int count, ObjectFlags flags);

Optional<Selection>
select_sentence(const Context& context, const Selection& selection,
                int count, ObjectFlags flags);

Optional<Selection>
select_paragraph(const Context& context, const Selection& selection,
                 int count, ObjectFlags flags);

Optional<Selection>
select_whitespaces(const Context& context, const Selection& selection,
                   int count, ObjectFlags flags);

Optional<Selection>
select_indent(const Context& context, const Selection& selection,
              int count, ObjectFlags flags);

Optional<Selection>
select_argument(const Context& context, const Selection& selection,
                int level, ObjectFlags flags);

Optional<Selection>
select_lines(const Context& context, const Selection& selection);

Optional<Selection>
trim_partial_lines(const Context& context, const Selection& selection);

enum class RegexMode;

template<RegexMode mode>
Selection find_next_match(const Context& context, const Selection& sel,
                          const Regex& regex, bool& wrapped);

Vector<Selection, MemoryDomain::Selections>
select_matches(const Buffer& buffer, ConstArrayView<Selection> selections,
               const Regex& regex, int capture_idx = 0);

Vector<Selection, MemoryDomain::Selections>
split_on_matches(const Buffer& buffer, ConstArrayView<Selection> selections,
                 const Regex& regex, int capture_idx = 0);

Optional<Selection>
select_surrounding(const Context& context, const Selection& selection,
                   const Regex& opening, const Regex& closing, int level,
                   ObjectFlags flags);

}

#endif // selectors_hh_INCLUDED
