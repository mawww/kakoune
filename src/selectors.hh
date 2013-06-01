#ifndef selectors_hh_INCLUDED
#define selectors_hh_INCLUDED

#include "selection.hh"
#include "unicode.hh"

namespace Kakoune
{

template<bool punctuation_is_word>
Selection select_to_next_word(const Buffer& buffer,
                              const Selection& selection);
template<bool punctuation_is_word>
Selection select_to_next_word_end(const Buffer& buffer,
                                  const Selection& selection);
template<bool punctuation_is_word>
Selection select_to_previous_word(const Buffer& buffer,
const Selection& selection);

Selection select_line(const Buffer& buffer,
const Selection& selection);
Selection select_matching(const Buffer& buffer,
                          const Selection& selection);

Selection select_to(const Buffer& buffer, const Selection& selection,
                    Codepoint c, int count, bool inclusive);
Selection select_to_reverse(const Buffer& buffer, const Selection& selection,
                            Codepoint c, int count, bool inclusive);

Selection select_to_eol(const Buffer& buffer, const Selection& selection);
Selection select_to_eol_reverse(const Buffer& buffer, const Selection& selection);

enum class ObjectFlags
{
    ToBegin = 1,
    ToEnd   = 2,
    Inner   = 4
};
constexpr bool operator&(ObjectFlags lhs, ObjectFlags rhs)
{ return (bool)((int)lhs & (int) rhs); }
constexpr ObjectFlags operator|(ObjectFlags lhs, ObjectFlags rhs)
{ return (ObjectFlags)((int)lhs | (int) rhs); }

template<bool punctuation_is_word>
Selection select_whole_word(const Buffer& buffer, const Selection& selection,
                            ObjectFlags flags);
Selection select_whole_sentence(const Buffer& buffer, const Selection& selection,
                                ObjectFlags flags);
Selection select_whole_paragraph(const Buffer& buffer, const Selection& selection,
                                 ObjectFlags flags);
Selection select_whole_lines(const Buffer& buffer, const Selection& selection);
Selection select_whole_buffer(const Buffer& buffer, const Selection& selection);
Selection trim_partial_lines(const Buffer& buffer, const Selection& selection);

template<bool forward>
Selection select_next_match(const Buffer& buffer, const Selection& selection,
                            const Regex& regex);

SelectionList select_all_matches(const Buffer& buffer, const Selection& selection,
                                 const Regex& regex);

SelectionList split_selection(const Buffer& buffer, const Selection& selection,
                              const Regex& separator_regex);

using CodepointPair = std::pair<Codepoint, Codepoint>;
Selection select_surrounding(const Buffer& buffer, const Selection& selection,
                             const CodepointPair& matching,
                             ObjectFlags flags);

}

#endif // selectors_hh_INCLUDED
