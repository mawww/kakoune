#include "selectors.hh"

#include "string.hh"
#include "utf8_iterator.hh"

#include <algorithm>

#include <boost/optional.hpp>

namespace Kakoune
{

using Utf8Iterator = utf8::utf8_iterator<BufferIterator, utf8::InvalidBytePolicy::Pass>;

namespace
{

template<WordType word_type = Word>
bool is_word(Codepoint c)
{
    return Kakoune::is_word(c);
}

template<>
bool is_word<WORD>(Codepoint c)
{
    return !is_blank(c) and !is_eol(c);
}

static bool is_punctuation(Codepoint c)
{
    return not (is_word(c) or is_blank(c) or is_eol(c));
}

enum class CharCategories
{
    Blank,
    EndOfLine,
    Word,
    Punctuation,
};

template<WordType word_type = Word>
CharCategories categorize(Codepoint c)
{
    if (is_word(c))
        return CharCategories::Word;
    if (is_eol(c))
        return CharCategories::EndOfLine;
    if (is_blank(c))
        return CharCategories::Blank;
    return word_type == WORD ? CharCategories::Word
                             : CharCategories::Punctuation;
}

template<typename Iterator, typename EndIterator, typename T>
void skip_while(Iterator& it, const EndIterator& end, T condition)
{
    while (it != end and condition(*it))
        ++it;
}

template<typename Iterator, typename BeginIterator, typename T>
void skip_while_reverse(Iterator& it, const BeginIterator& begin, T condition)
{
    while (it != begin and condition(*it))
        --it;
}

Range utf8_range(const Utf8Iterator& first, const Utf8Iterator& last)
{
    return {first.base().coord(), last.base().coord()};
}

}

typedef boost::regex_iterator<BufferIterator> RegexIterator;

template<WordType word_type>
Selection select_to_next_word(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.last());
    if (begin+1 == buffer.end())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin+1)))
        ++begin;

    skip_while(begin, buffer.end(), is_eol);
    if (begin == buffer.end())
        return selection;
    Utf8Iterator end = begin+1;

    if (word_type == Word and is_punctuation(*begin))
        skip_while(end, buffer.end(), is_punctuation);
    else if (is_word<word_type>(*begin))
        skip_while(end, buffer.end(), is_word<word_type>);

    skip_while(end, buffer.end(), is_blank);

    return utf8_range(begin, end-1);
}
template Selection select_to_next_word<Word>(const Buffer&, const Selection&);
template Selection select_to_next_word<WORD>(const Buffer&, const Selection&);

template<WordType word_type>
Selection select_to_next_word_end(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.last());
    if (begin+1 == buffer.end())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin+1)))
        ++begin;

    skip_while(begin, buffer.end(), is_eol);
    if (begin == buffer.end())
        return selection;
    Utf8Iterator end = begin;
    skip_while(end, buffer.end(), is_blank);

    if (word_type == Word and is_punctuation(*end))
        skip_while(end, buffer.end(), is_punctuation);
    else if (is_word<word_type>(*end))
        skip_while(end, buffer.end(), is_word<word_type>);

    return utf8_range(begin, end-1);
}
template Selection select_to_next_word_end<Word>(const Buffer&, const Selection&);
template Selection select_to_next_word_end<WORD>(const Buffer&, const Selection&);

template<WordType word_type>
Selection select_to_previous_word(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.last());
    if (begin+1 == buffer.end())
        return selection;
    if (categorize<word_type>(*begin) != categorize<word_type>(*(begin-1)))
        --begin;

    skip_while_reverse(begin, buffer.begin(), is_eol);
    Utf8Iterator end = begin;
    skip_while_reverse(end, buffer.begin(), is_blank);

    bool with_end = false;
    if (word_type == Word and is_punctuation(*end))
    {
        skip_while_reverse(end, buffer.begin(), is_punctuation);
        with_end = is_punctuation(*end);
    }
    else if (is_word<word_type>(*end))
    {
        skip_while_reverse(end, buffer.begin(), is_word<word_type>);
        with_end = is_word<word_type>(*end);
    }

    return utf8_range(begin, with_end ? end : end+1);
}
template Selection select_to_previous_word<Word>(const Buffer&, const Selection&);
template Selection select_to_previous_word<WORD>(const Buffer&, const Selection&);

Selection select_line(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator first = buffer.iterator_at(selection.last());
    if (*first == '\n' and first + 1 != buffer.end())
        ++first;

    while (first != buffer.begin() and *(first - 1) != '\n')
        --first;

    Utf8Iterator last = first;
    while (last + 1 != buffer.end() and *last != '\n')
        ++last;
    return utf8_range(first, last);
}

Selection select_matching(const Buffer& buffer, const Selection& selection)
{
    std::vector<Codepoint> matching_pairs = { '(', ')', '{', '}', '[', ']', '<', '>' };
    Utf8Iterator it = buffer.iterator_at(selection.last());
    std::vector<Codepoint>::iterator match = matching_pairs.end();
    while (not is_eol(*it))
    {
        match = std::find(matching_pairs.begin(), matching_pairs.end(), *it);
        if (match != matching_pairs.end())
            break;
        ++it;
    }
    if (match == matching_pairs.end())
        return selection;

    Utf8Iterator begin = it;

    if (((match - matching_pairs.begin()) % 2) == 0)
    {
        int level = 0;
        const Codepoint opening = *match;
        const Codepoint closing = *(match+1);
        while (it != buffer.end())
        {
            if (*it == opening)
                ++level;
            else if (*it == closing and --level == 0)
                return utf8_range(begin, it);
            ++it;
        }
    }
    else
    {
        int level = 0;
        const Codepoint opening = *(match-1);
        const Codepoint closing = *match;
        while (true)
        {
            if (*it == closing)
                ++level;
            else if (*it == opening and --level == 0)
                return utf8_range(begin, it);
            if (it == buffer.begin())
                break;
            --it;
        }
    }
    return selection;
}

// c++14 will add std::optional, so we use boost::optional until then
using boost::optional;
static optional<Range> find_surrounding(const Buffer& buffer,
                                        BufferCoord coord,
                                        CodepointPair matching,
                                        ObjectFlags flags, int init_level)
{
    const bool to_begin = flags & ObjectFlags::ToBegin;
    const bool to_end   = flags & ObjectFlags::ToEnd;
    const bool nestable = matching.first != matching.second;
    auto pos = buffer.iterator_at(coord);
    Utf8Iterator first = pos;
    if (to_begin)
    {
        int level = nestable ? init_level : 0;
        while (first != buffer.begin())
        {
            if (nestable and first != pos and *first == matching.second)
                ++level;
            else if (*first == matching.first)
            {
                if (level == 0)
                    break;
                else
                    --level;
            }
            --first;
        }
        if (level != 0 or *first != matching.first)
            return optional<Range>{};
    }

    Utf8Iterator last = pos;
    if (to_end)
    {
        int level = nestable ? init_level : 0;
        while (last != buffer.end())
        {
            if (nestable and last != pos and *last == matching.first)
                ++level;
            else if (*last == matching.second)
            {
                if (level == 0)
                    break;
                else
                    --level;
            }
            ++last;
        }
        if (level != 0 or last == buffer.end())
            return optional<Range>{};
    }

    if (flags & ObjectFlags::Inner)
    {
        if (to_begin and first != last)
            ++first;
        if (to_end and first != last)
            --last;
    }
    return to_end ? utf8_range(first, last) : utf8_range(last, first);
}

Selection select_surrounding(const Buffer& buffer, const Selection& selection,
                             CodepointPair matching, int level,
                             ObjectFlags flags)
{
	const bool nestable = matching.first != matching.second;
	auto pos = selection.last();
	if (not nestable or flags & ObjectFlags::Inner)
	{
        if (auto res = find_surrounding(buffer, pos, matching, flags, level))
            return *res;
        return selection;
	}

	auto c = buffer.byte_at(pos);
    if ((flags == ObjectFlags::ToBegin and c == matching.first) or
        (flags == ObjectFlags::ToEnd and c == matching.second))
        ++level;

    auto res = find_surrounding(buffer, pos, matching, flags, level);
    if (not res)
        return selection;

    if (flags == (ObjectFlags::ToBegin | ObjectFlags::ToEnd) and
        res->min() == selection.min() and res->max() == selection.max())
    {
        if (auto res_parent = find_surrounding(buffer, pos, matching, flags, level+1))
            return Selection{*res_parent};
    }
    return *res;
}

Selection select_to(const Buffer& buffer, const Selection& selection,
                    Codepoint c, int count, bool inclusive)
{
    Utf8Iterator begin = buffer.iterator_at(selection.last());
    Utf8Iterator end = begin;
    do
    {
        ++end;
        skip_while(end, buffer.end(), [c](Codepoint cur) { return cur != c; });
        if (end == buffer.end())
            return selection;
    }
    while (--count > 0);

    return utf8_range(begin, inclusive ? end : end-1);
}

Selection select_to_reverse(const Buffer& buffer, const Selection& selection,
                            Codepoint c, int count, bool inclusive)
{
    Utf8Iterator begin = buffer.iterator_at(selection.last());
    Utf8Iterator end = begin;
    do
    {
        --end;
        skip_while_reverse(end, buffer.begin(), [c](Codepoint cur) { return cur != c; });
        if (end == buffer.begin())
            return selection;
    }
    while (--count > 0);

    return utf8_range(begin, inclusive ? end : end+1);
}

Selection select_to_eol(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.last());
    Utf8Iterator end = begin + 1;
    skip_while(end, buffer.end(), [](Codepoint cur) { return not is_eol(cur); });
    return utf8_range(begin, end-1);
}

Selection select_to_eol_reverse(const Buffer& buffer, const Selection& selection)
{
    Utf8Iterator begin = buffer.iterator_at(selection.last());
    Utf8Iterator end = begin - 1;
    skip_while_reverse(end, buffer.begin(), [](Codepoint cur) { return not is_eol(cur); });
    return utf8_range(begin, end == buffer.begin() ? end : end+1);
}

template<WordType word_type>
Selection select_whole_word(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    Utf8Iterator first = buffer.iterator_at(selection.last());
    Utf8Iterator last = first;
    if (is_word<word_type>(*first))
    {
        if (flags & ObjectFlags::ToBegin)
        {
            skip_while_reverse(first, buffer.begin(), is_word<word_type>);
            if (not is_word<word_type>(*first))
                ++first;
        }
        if (flags & ObjectFlags::ToEnd)
        {
            skip_while(last, buffer.end(), is_word<word_type>);
            if (not (flags & ObjectFlags::Inner))
                skip_while(last, buffer.end(), is_blank);
            --last;
        }
    }
    else if (not (flags & ObjectFlags::Inner))
    {
        if (flags & ObjectFlags::ToBegin)
        {
            skip_while_reverse(first, buffer.begin(), is_blank);
            if (not is_word<word_type>(*first))
                return selection;
            skip_while_reverse(first, buffer.begin(), is_word<word_type>);
            if (not is_word<word_type>(*first))
                ++first;
        }
        if (flags & ObjectFlags::ToEnd)
        {
            skip_while(last, buffer.end(), is_blank);
            --last;
        }
    }
    return (flags & ObjectFlags::ToEnd) ? utf8_range(first, last)
                                        : utf8_range(last, first);
}
template Selection select_whole_word<Word>(const Buffer&, const Selection&, ObjectFlags);
template Selection select_whole_word<WORD>(const Buffer&, const Selection&, ObjectFlags);

Selection select_whole_sentence(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    BufferIterator first = buffer.iterator_at(selection.last());
    BufferIterator last = first;

    if (flags & ObjectFlags::ToBegin)
    {
        bool saw_non_blank = false;
        while (first != buffer.begin())
        {
            char cur = *first;
            char prev = *(first-1);
            if (not is_blank(cur))
                saw_non_blank = true;
            if (is_eol(prev) and is_eol(cur))
            {
                ++first;
                break;
            }
            else if (prev == '.' or prev == ';' or prev == '!' or prev == '?')
            {
                if (saw_non_blank)
                    break;
                else if (flags & ObjectFlags::ToEnd)
                    last = first-1;
            }
            --first;
        }
        skip_while(first, buffer.end(), is_blank);
    }
    if (flags & ObjectFlags::ToEnd)
    {
        while (last != buffer.end())
        {
            char cur = *last;
            if (cur == '.' or cur == ';' or cur == '!' or cur == '?' or
                (is_eol(cur) and (last+1 == buffer.end() or is_eol(*(last+1)))))
                break;
            ++last;
        }
        if (not (flags & ObjectFlags::Inner) and last != buffer.end())
        {
            ++last;
            skip_while(last, buffer.end(), is_blank);
            --last;
        }
    }
    return (flags & ObjectFlags::ToEnd) ? Selection{first.coord(), last.coord()}
                                        : Selection{last.coord(), first.coord()};
}

Selection select_whole_paragraph(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    BufferIterator first = buffer.iterator_at(selection.last());
    BufferIterator last = first;

    if (flags & ObjectFlags::ToBegin and first != buffer.begin())
    {
        skip_while_reverse(first, buffer.begin(), is_eol);
        if (flags & ObjectFlags::ToEnd)
            last = first;
        while (first != buffer.begin())
        {
            char cur = *first;
            char prev = *(first-1);
            if (is_eol(prev) and is_eol(cur))
            {
                ++first;
                break;
            }
            --first;
        }
    }
    if (flags & ObjectFlags::ToEnd)
    {
        while (last != buffer.end())
        {
            char cur = *last;
            char prev = *(last-1);
            if (is_eol(cur) and is_eol(prev))
            {
                if (not (flags & ObjectFlags::Inner))
                    skip_while(last, buffer.end(), is_eol);
                break;
            }
            ++last;
        }
        --last;
    }
    return (flags & ObjectFlags::ToEnd) ? Selection{first.coord(), last.coord()}
                                        : Selection{last.coord(), first.coord()};
}

static CharCount get_indent(const String& str, int tabstop)
{
    CharCount indent = 0;
    for (auto& c : str)
    {
    if (c == ' ')
        ++indent;
    else if (c =='\t')
        indent = (indent / tabstop + 1) * tabstop;
    else
        break;
    }
    return indent;
}

Selection select_whole_indent(const Buffer& buffer, const Selection& selection, ObjectFlags flags)
{
    int tabstop = buffer.options()["tabstop"].get<int>();
    LineCount line = selection.last().line;
    auto indent = get_indent(buffer[line], tabstop);

    LineCount begin_line = line - 1;
    if (flags & ObjectFlags::ToBegin)
    {
        while (begin_line >= 0 and (buffer[begin_line] == "\n" or get_indent(buffer[begin_line], tabstop) >= indent))
            --begin_line;
    }
    ++begin_line;
    LineCount end_line = line + 1;
    if (flags & ObjectFlags::ToEnd)
    {
        LineCount end = buffer.line_count();
        while (end_line < end and (buffer[end_line] == "\n" or get_indent(buffer[end_line], tabstop) >= indent))
            ++end_line;
    }
    --end_line;
    BufferCoord first = begin_line;
    // keep the first line indent in inner mode
    if (flags & ObjectFlags::Inner)
    {
        CharCount i = 0;
        for (; i < indent; ++first.column)
        {
            auto c = buffer.byte_at(first);
            if (c == ' ')
                ++i;
            if (c == '\t')
                i = (i / tabstop + 1) * tabstop;
        }
    }
    return Selection{first, {end_line, buffer[end_line].length() - 1}};
}

Selection select_whole_lines(const Buffer& buffer, const Selection& selection)
{
    // no need to be utf8 aware for is_eol as we only use \n as line seperator
    BufferIterator first = buffer.iterator_at(selection.first());
    BufferIterator last  = buffer.iterator_at(selection.last());
    BufferIterator& to_line_start = first <= last ? first : last;
    BufferIterator& to_line_end = first <= last ? last : first;

    --to_line_start;
    skip_while_reverse(to_line_start, buffer.begin(), [](char cur) { return not is_eol(cur); });
    if (is_eol(*to_line_start))
        ++to_line_start;

    skip_while(to_line_end, buffer.end(), [](char cur) { return not is_eol(cur); });
    if (to_line_end == buffer.end())
        --to_line_end;

    return Selection(first.coord(), last.coord());
}

Selection trim_partial_lines(const Buffer& buffer, const Selection& selection)
{
    // same as select_whole_lines
    BufferIterator first = buffer.iterator_at(selection.first());
    BufferIterator last =  buffer.iterator_at(selection.last());
    BufferIterator& to_line_start = first <= last ? first : last;
    BufferIterator& to_line_end = first <= last ? last : first;

    while (to_line_start != buffer.begin() and *(to_line_start-1) != '\n')
        ++to_line_start;
    while (*(to_line_end+1) != '\n' and to_line_end != to_line_start)
        --to_line_end;

    return Selection(first.coord(), last.coord());
}

Selection select_whole_buffer(const Buffer& buffer, const Selection&)
{
    return Selection({0,0}, buffer.back_coord());
}

using MatchResults = boost::match_results<BufferIterator>;

static bool find_last_match(BufferIterator begin, const BufferIterator& end,
                            MatchResults& res, const Regex& regex)
{
    MatchResults matches;
    while (boost::regex_search(begin, end, matches, regex))
    {
        if (begin == matches[0].second)
            break;
        begin = matches[0].second;
        res.swap(matches);
    }
    return not res.empty();
}

template<Direction direction>
bool find_match_in_buffer(const Buffer& buffer, const BufferIterator pos,
                          MatchResults& matches, const Regex& ex)
{
    if (direction == Forward)
        return (boost::regex_search(pos, buffer.end(), matches, ex) or
                boost::regex_search(buffer.begin(), pos, matches, ex));
    else
        return (find_last_match(buffer.begin(), pos, matches, ex) or
                find_last_match(pos, buffer.end(), matches, ex));
}


template<Direction direction>
Selection select_next_match(const Buffer& buffer, const Selection& selection, const Regex& regex)
{
    // regex matching do not use Utf8Iterator as boost::regex handle utf8
    // decoding itself
    BufferIterator begin = buffer.iterator_at(selection.last());
    BufferIterator end = begin;
    CaptureList captures;

    MatchResults matches;

    bool found = false;
    if ((found = find_match_in_buffer<direction>(buffer, utf8::next(begin), matches, regex)))
    {
        begin = matches[0].first;
        end   = matches[0].second;
        for (auto& match : matches)
            captures.push_back(String(match.first, match.second));
    }
    if (not found or begin == buffer.end())
        throw runtime_error("'" + regex.str() + "': no matches found");

    end = (begin == end) ? end : utf8::previous(end);
    if (direction == Backward)
        std::swap(begin, end);
    return Selection{begin.coord(), end.coord(), std::move(captures)};
}
template Selection select_next_match<Forward>(const Buffer&, const Selection&, const Regex&);
template Selection select_next_match<Backward>(const Buffer&, const Selection&, const Regex&);

SelectionList select_all_matches(const Buffer& buffer, const Selection& selection, const Regex& regex)
{
    auto sel_end = utf8::next(buffer.iterator_at(selection.max()));
    RegexIterator re_it(buffer.iterator_at(selection.min()), sel_end, regex);
    RegexIterator re_end;

    SelectionList result;
    for (; re_it != re_end; ++re_it)
    {
        BufferIterator begin = (*re_it)[0].first;
        BufferIterator end   = (*re_it)[0].second;

        if (begin == sel_end)
            continue;

        CaptureList captures;
        for (auto& match : *re_it)
            captures.push_back(String(match.first, match.second));

        result.push_back(Selection(begin.coord(), (begin == end ? end : utf8::previous(end)).coord(),
                                   std::move(captures)));
    }
    return result;
}

SelectionList split_selection(const Buffer& buffer, const Selection& selection,
                              const Regex& regex)
{
    auto begin = buffer.iterator_at(selection.min());
    auto sel_end = utf8::next(buffer.iterator_at(selection.max()));
    RegexIterator re_it(begin, sel_end, regex, boost::regex_constants::match_nosubs);
    RegexIterator re_end;

    SelectionList result;
    for (; re_it != re_end; ++re_it)
    {
        BufferIterator end = (*re_it)[0].first;

        result.push_back(Selection(begin.coord(), (begin == end) ? end.coord() : utf8::previous(end).coord()));
        begin = (*re_it)[0].second;
    }
    result.push_back(Selection(begin.coord(), selection.max()));
    return result;
}

}
