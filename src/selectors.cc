#include "selectors.hh"

#include "string.hh"

#include "utf8_iterator.hh"

#include <algorithm>

namespace Kakoune
{

using Utf8Iterator = utf8::utf8_iterator<BufferIterator, utf8::InvalidBytePolicy::Pass>;

namespace
{

template<bool punctuation_is_word = false>
bool is_word(Codepoint c)
{
    return Kakoune::is_word(c);
}

template<>
bool is_word<true>(Codepoint c)
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

template<bool punctuation_is_word = false>
CharCategories categorize(Codepoint c)
{
    if (is_word(c))
        return CharCategories::Word;
    if (is_eol(c))
        return CharCategories::EndOfLine;
    if (is_blank(c))
        return CharCategories::Blank;
    return punctuation_is_word ? CharCategories::Word
                               : CharCategories::Punctuation;
}

bool is_begin(const BufferIterator& it) { return it.is_begin(); }
bool is_end(const BufferIterator& it) { return it.is_end(); }

bool is_begin(const Utf8Iterator& it) { return it.underlying_iterator().is_begin(); }
bool is_end(const Utf8Iterator& it) { return it.underlying_iterator().is_end(); }

template<typename Iterator, typename T>
void skip_while(Iterator& it, T condition)
{
    while (not is_end(it) and condition(*it))
        ++it;
}

template<typename Iterator, typename T>
void skip_while_reverse(Iterator& it, T condition)
{
    while (not is_begin(it) and condition(*it))
        --it;
}

Selection utf8_selection(const Utf8Iterator& first, const Utf8Iterator& last)
{
    return Selection(first.underlying_iterator(), last.underlying_iterator());
}

}

typedef boost::regex_iterator<BufferIterator> RegexIterator;

template<bool punctuation_is_word>
Selection select_to_next_word(const Selection& selection)
{
    Utf8Iterator begin = selection.last();
    if (categorize<punctuation_is_word>(*begin) !=
        categorize<punctuation_is_word>(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);
    Utf8Iterator end = begin+1;

    if (not punctuation_is_word and is_punctuation(*begin))
        skip_while(end, is_punctuation);
    else if (is_word<punctuation_is_word>(*begin))
        skip_while(end, is_word<punctuation_is_word>);

    skip_while(end, is_blank);

    return utf8_selection(begin, end-1);
}
template Selection select_to_next_word<false>(const Selection&);
template Selection select_to_next_word<true>(const Selection&);

template<bool punctuation_is_word>
Selection select_to_next_word_end(const Selection& selection)
{
    Utf8Iterator begin = selection.last();
    if (categorize<punctuation_is_word>(*begin) !=
        categorize<punctuation_is_word>(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);
    Utf8Iterator end = begin;
    skip_while(end, is_blank);

    if (not punctuation_is_word and is_punctuation(*end))
        skip_while(end, is_punctuation);
    else if (is_word<punctuation_is_word>(*end))
        skip_while(end, is_word<punctuation_is_word>);

    return utf8_selection(begin, end-1);
}
template Selection select_to_next_word_end<false>(const Selection&);
template Selection select_to_next_word_end<true>(const Selection&);

template<bool punctuation_is_word>
Selection select_to_previous_word(const Selection& selection)
{
    Utf8Iterator begin = selection.last();

    if (categorize<punctuation_is_word>(*begin) !=
        categorize<punctuation_is_word>(*(begin-1)))
        --begin;

    skip_while_reverse(begin, is_eol);
    Utf8Iterator end = begin;
    skip_while_reverse(end, is_blank);

    bool with_end = false;
    if (not punctuation_is_word and is_punctuation(*end))
    {
        skip_while_reverse(end, is_punctuation);
        with_end = is_punctuation(*end);
    }
    else if (is_word<punctuation_is_word>(*end))
    {
        skip_while_reverse(end, is_word<punctuation_is_word>);
        with_end = is_word<punctuation_is_word>(*end);
    }

    return utf8_selection(begin, with_end ? end : end+1);
}
template Selection select_to_previous_word<false>(const Selection&);
template Selection select_to_previous_word<true>(const Selection&);

Selection select_line(const Selection& selection)
{
    Utf8Iterator first = selection.last();
    if (*first == '\n' and not is_end(first + 1))
        ++first;

    while (not is_begin(first) and *(first - 1) != '\n')
        --first;

    Utf8Iterator last = first;
    while (not is_end(last + 1) and *last != '\n')
        ++last;
    return utf8_selection(first, last);
}

Selection select_matching(const Selection& selection)
{
    std::vector<Codepoint> matching_pairs = { '(', ')', '{', '}', '[', ']', '<', '>' };
    Utf8Iterator it = selection.last();
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
        while (not is_end(it))
        {
            if (*it == opening)
                ++level;
            else if (*it == closing and --level == 0)
                return utf8_selection(begin, it);

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
                return utf8_selection(begin, it);
            if (is_begin(it))
                break;
            --it;
        }
    }
    return selection;
}

Selection select_surrounding(const Selection& selection,
                             const CodepointPair& matching,
                             SurroundFlags flags)
{
    const bool to_begin = flags & SurroundFlags::ToBegin;
    const bool to_end   = flags & SurroundFlags::ToEnd;
    Utf8Iterator first = selection.last();
    if (to_begin)
    {
        int level = 0;
        while (not is_begin(first))
        {
            if (first != selection.last() and *first == matching.second)
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
            return selection;
    }

    Utf8Iterator last = selection.last();
    if (to_end)
    {
        int level = 0;
        last = first + 1;
        while (not is_end(last))
        {
            if (*last == matching.first)
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
        if (level != 0 or *last != matching.second)
            return selection;
    }

    if (flags & SurroundFlags::Inner)
    {
        if (to_begin)
            ++first;
        if (to_end and first != last)
            --last;
    }
    return to_end ? utf8_selection(first, last) : utf8_selection(last, first);
}

Selection select_to(const Selection& selection,
                    Codepoint c, int count, bool inclusive)
{
    Utf8Iterator begin = selection.last();
    Utf8Iterator end = begin;
    do
    {
        ++end;
        skip_while(end, [c](Codepoint cur) { return not is_eol(cur) and cur != c; });
        if (is_end(end) or is_eol(*end))
            return selection;
    }
    while (--count > 0);

    return utf8_selection(begin, inclusive ? end : end-1);
}

Selection select_to_reverse(const Selection& selection,
                            Codepoint c, int count, bool inclusive)
{
    Utf8Iterator begin = selection.last();
    Utf8Iterator end = begin;
    do
    {
        --end;
        skip_while_reverse(end, [c](Codepoint cur) { return not is_eol(cur) and cur != c; });
        if (is_begin(end) or is_eol(*end))
            return selection;
    }
    while (--count > 0);

    return utf8_selection(begin, inclusive ? end : end+1);
}

Selection select_to_eol(const Selection& selection)
{
    Utf8Iterator begin = selection.last();
    Utf8Iterator end = begin + 1;
    skip_while(end, [](Codepoint cur) { return not is_eol(cur); });
    return utf8_selection(begin, end-1);
}

Selection select_to_eol_reverse(const Selection& selection)
{
    Utf8Iterator begin = selection.last();
    Utf8Iterator end = begin - 1;
    skip_while_reverse(end, [](Codepoint cur) { return not is_eol(cur); });
    return utf8_selection(begin, is_begin(end) ? end : end+1);
}

template<bool punctuation_is_word>
Selection select_whole_word(const Selection& selection, bool inner)
{
    Utf8Iterator first = selection.last();
    Utf8Iterator last = first;
    if (is_word(*first))
    {
        skip_while_reverse(first, is_word<punctuation_is_word>);
        if (not is_word<punctuation_is_word>(*first))
            ++first;
        skip_while(last, is_word<punctuation_is_word>);
        if (not inner)
            skip_while(last, is_blank);
    }
    else if (not inner)
    {
        skip_while_reverse(first, is_blank);
        if (is_blank(*first))
            ++first;
        skip_while(last, is_blank);
        if (not is_word<punctuation_is_word>(*last))
            return selection;
        skip_while(last, is_word<punctuation_is_word>);
    }
    --last;
    return utf8_selection(first, last);
}
template Selection select_whole_word<false>(const Selection&, bool);
template Selection select_whole_word<true>(const Selection&, bool);

Selection select_whole_lines(const Selection& selection)
{
    // no need to be utf8 aware for is_eol as we only use \n as line seperator
    BufferIterator first = selection.first();
    BufferIterator last =  selection.last();
    BufferIterator& to_line_start = first <= last ? first : last;
    BufferIterator& to_line_end = first <= last ? last : first;

    --to_line_start;
    skip_while_reverse(to_line_start, [](char cur) { return not is_eol(cur); });
    if (is_eol(*to_line_start))
        ++to_line_start;

    skip_while(to_line_end, [](char cur) { return not is_eol(cur); });

    return Selection(first, last);
}

Selection select_whole_buffer(const Selection& selection)
{
    const Buffer& buffer = selection.first().buffer();
    return Selection(buffer.begin(), utf8::previous(buffer.end()));
}

Selection select_next_match(const Selection& selection, const String& regex)
{
    try
    {
        // regex matching do not use Utf8Iterator as boost::regex handle utf8
        // decoding itself
        BufferIterator begin = selection.last();
        BufferIterator end = begin;
        CaptureList captures;

        Regex ex(regex.begin(), regex.end());
        boost::match_results<BufferIterator> matches;

        if (boost::regex_search(utf8::next(begin), begin.buffer().end(),
                                matches, ex))
        {
            begin = matches[0].first;
            end   = matches[0].second;
            for (auto& match : matches)
                captures.push_back(String(match.first, match.second));
        }
        else if (boost::regex_search(begin.buffer().begin(), utf8::next(begin),
                                     matches, ex))
        {
            begin = matches[0].first;
            end   = matches[0].second;
            for (auto& match : matches)
                captures.push_back(String(match.first, match.second));
        }
        else
            throw runtime_error("'" + regex + "': no matches found");

        if (begin == end)
            ++end;

        return Selection(begin, utf8::previous(end), std::move(captures));
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

SelectionList select_all_matches(const Selection& selection, const String& regex)
{
    try
    {
        Regex ex(regex.begin(), regex.end());
        RegexIterator re_it(selection.begin(), selection.end(), ex);
        RegexIterator re_end;

        SelectionList result;
        for (; re_it != re_end; ++re_it)
        {
            BufferIterator begin = (*re_it)[0].first;
            BufferIterator end   = (*re_it)[0].second;

            if (begin == selection.end())
                continue;

            CaptureList captures;
            for (auto& match : *re_it)
                captures.push_back(String(match.first, match.second));

            result.push_back(Selection(begin, begin == end ? end : utf8::previous(end),
                                       std::move(captures)));
        }
        return result;
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

SelectionList split_selection(const Selection& selection,
                              const String& separator_regex)
{
    try
    {
        Regex ex(separator_regex.begin(), separator_regex.end());
        RegexIterator re_it(selection.begin(), selection.end(), ex,
                            boost::regex_constants::match_nosubs);
        RegexIterator re_end;

        SelectionList result;
        BufferIterator begin = selection.begin();
        for (; re_it != re_end; ++re_it)
        {
            BufferIterator end = (*re_it)[0].first;

            result.push_back(Selection(begin, (begin == end) ? end : utf8::previous(end)));
            begin = (*re_it)[0].second;
        }
        result.push_back(Selection(begin, std::max(selection.first(),
                                                   selection.last())));
        return result;
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

}
