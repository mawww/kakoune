#include "selectors.hh"

#include "string.hh"

#include <algorithm>

namespace Kakoune
{

namespace
{

bool is_eol(char c)
{
    return c == '\n';
}

bool is_blank(char c)
{
    return c == ' ' or c == '\t';
}

template<bool punctuation_is_word = false>
bool is_word(char c);

template<>
bool is_word<false>(char c)
{
    if (c >= '0' and c <= '9')
        return true;
    if (c >= 'a' and c <= 'z')
        return true;
    if (c >= 'A' and c <= 'Z')
        return true;
    if (c == '_')
        return true;
    return false;
}

template<>
bool is_word<true>(char c)
{
    return !is_blank(c) and !is_eol(c);
}

static bool is_punctuation(char c)
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
CharCategories categorize(char c)
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

template<typename T>
bool skip_while(BufferIterator& it, T condition)
{
    while (not it.is_end() and condition(*it))
        ++it;
    return not it.is_end() and condition(*it);
}

template<typename T>
bool skip_while_reverse(BufferIterator& it, T condition)
{
    while (not it.is_begin() and condition(*it))
        --it;
    return not it.is_end() and condition(*it);
}

}

typedef boost::regex_iterator<BufferIterator> RegexIterator;

template<bool punctuation_is_word>
SelectionAndCaptures select_to_next_word(const Selection& selection)
{
    BufferIterator begin = selection.last();
    if (categorize<punctuation_is_word>(*begin) !=
        categorize<punctuation_is_word>(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);
    BufferIterator end = begin+1;

    if (not punctuation_is_word and is_punctuation(*begin))
        skip_while(end, is_punctuation);
    else if (is_word<punctuation_is_word>(*begin))
        skip_while(end, is_word<punctuation_is_word>);

    bool with_end = skip_while(end, is_blank);

    return Selection(begin, with_end ? end : end-1);
}
template SelectionAndCaptures select_to_next_word<false>(const Selection&);
template SelectionAndCaptures select_to_next_word<true>(const Selection&);

template<bool punctuation_is_word>
SelectionAndCaptures select_to_next_word_end(const Selection& selection)
{
    BufferIterator begin = selection.last();
    if (categorize<punctuation_is_word>(*begin) !=
        categorize<punctuation_is_word>(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);
    BufferIterator end = begin;
    skip_while(end, is_blank);

    bool with_end = false;
    if (not punctuation_is_word and is_punctuation(*end))
        with_end = skip_while(end, is_punctuation);
    else if (is_word<punctuation_is_word>(*end))
        with_end = skip_while(end, is_word<punctuation_is_word>);

    return Selection(begin, with_end ? end : end-1);
}
template SelectionAndCaptures select_to_next_word_end<false>(const Selection&);
template SelectionAndCaptures select_to_next_word_end<true>(const Selection&);

template<bool punctuation_is_word>
SelectionAndCaptures select_to_previous_word(const Selection& selection)
{
    BufferIterator begin = selection.last();

    if (categorize<punctuation_is_word>(*begin) !=
        categorize<punctuation_is_word>(*(begin-1)))
        --begin;

    skip_while_reverse(begin, is_eol);
    BufferIterator end = begin;
    skip_while_reverse(end, is_blank);

    bool with_end = false;
    if (not punctuation_is_word and is_punctuation(*end))
        with_end = skip_while_reverse(end, is_punctuation);
    else if (is_word<punctuation_is_word>(*end))
        with_end = skip_while_reverse(end, is_word<punctuation_is_word>);

    return Selection(begin, with_end ? end : end+1);
}
template SelectionAndCaptures select_to_previous_word<false>(const Selection&);
template SelectionAndCaptures select_to_previous_word<true>(const Selection&);

SelectionAndCaptures select_line(const Selection& selection)
{
    BufferIterator first = selection.last();
    if (*first == '\n' and not (first + 1).is_end())
        ++first;

    while (not first.is_begin() and *(first - 1) != '\n')
        --first;

    BufferIterator last = first;
    while (not (last + 1).is_end() and *last != '\n')
        ++last;
    return Selection(first, last);
}

SelectionAndCaptures select_matching(const Selection& selection)
{
    std::vector<char> matching_pairs = { '(', ')', '{', '}', '[', ']', '<', '>' };
    BufferIterator it = selection.last();
    std::vector<char>::iterator match = matching_pairs.end();
    while (not is_eol(*it))
    {
        match = std::find(matching_pairs.begin(), matching_pairs.end(), *it);
        if (match != matching_pairs.end())
            break;
        ++it;
    }
    if (match == matching_pairs.end())
        return selection;

    BufferIterator begin = it;

    if (((match - matching_pairs.begin()) % 2) == 0)
    {
        int level = 0;
        const char opening = *match;
        const char closing = *(match+1);
        while (not it.is_end())
        {
            if (*it == opening)
                ++level;
            else if (*it == closing and --level == 0)
                return Selection(begin, it);

            ++it;
        }
    }
    else
    {
        int level = 0;
        const char opening = *(match-1);
        const char closing = *match;
        while (not it.is_begin())
        {
            if (*it == closing)
                ++level;
            else if (*it == opening and --level == 0)
                return Selection(begin, it);
            --it;
        }
    }
    return selection;
}

SelectionAndCaptures select_surrounding(const Selection& selection,
                             const std::pair<char, char>& matching,
                             bool inside)
{
    int level = 0;
    BufferIterator first = selection.last();
    while (not first.is_begin())
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

    level = 0;
    BufferIterator last = first + 1;
    while (not last.is_end())
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

    if (inside)
    {
        ++first;
        if (first != last)
            --last;
    }
    return Selection(first, last);
}

SelectionAndCaptures select_to(const Selection& selection,
                               char c, int count, bool inclusive)
{
    BufferIterator begin = selection.last();
    BufferIterator end = begin;
    do
    {
        ++end;
        skip_while(end, [c](char cur) { return not is_eol(cur) and cur != c; });
        if (end.is_end() or is_eol(*end))
            return selection;
    }
    while (--count > 0);

    return Selection(begin, inclusive ? end : end-1);
}

SelectionAndCaptures select_to_reverse(const Selection& selection,
                                       char c, int count, bool inclusive)
{
    BufferIterator begin = selection.last();
    BufferIterator end = begin;
    do
    {
        --end;
        skip_while_reverse(end, [c](char cur) { return not is_eol(cur) and cur != c; });
        if (end.is_begin() or is_eol(*end))
            return selection;
    }
    while (--count > 0);

    return Selection(begin, inclusive ? end : end+1);
}

SelectionAndCaptures select_to_eol(const Selection& selection)
{
    BufferIterator begin = selection.last();
    BufferIterator end = begin + 1;
    skip_while(end, [](char cur) { return not is_eol(cur); });
    return Selection(begin, end-1);
}

SelectionAndCaptures select_to_eol_reverse(const Selection& selection)
{
    BufferIterator begin = selection.last();
    BufferIterator end = begin - 1;
    skip_while_reverse(end, [](char cur) { return not is_eol(cur); });
    return Selection(begin, end.is_begin() ? end : end+1);
}

template<bool punctuation_is_word>
SelectionAndCaptures select_whole_word(const Selection& selection, bool inner)
{
    BufferIterator first = selection.last();
    BufferIterator last = first;
    if (is_word(*first))
    {
        if (not skip_while_reverse(first, is_word<punctuation_is_word>))
            ++first;
        skip_while(last, is_word<punctuation_is_word>);
        if (not inner)
            skip_while(last, is_blank);
    }
    else if (not inner)
    {
        if (not skip_while_reverse(first, is_blank))
            ++first;
        skip_while(last, is_blank);
        if (not is_word<punctuation_is_word>(*last))
            return selection;
        skip_while(last, is_word<punctuation_is_word>);
    }
    --last;
    return Selection(first, last);
}
template SelectionAndCaptures select_whole_word<false>(const Selection&, bool);
template SelectionAndCaptures select_whole_word<true>(const Selection&, bool);

SelectionAndCaptures select_whole_lines(const Selection& selection)
{
     BufferIterator first = selection.first();
     BufferIterator last =  selection.last();
     BufferIterator& to_line_start = first <= last ? first : last;
     BufferIterator& to_line_end = first <= last ? last : first;

     --to_line_start;
     skip_while_reverse(to_line_start, [](char cur) { return not is_eol(cur); });
     ++to_line_start;

     skip_while(to_line_end, [](char cur) { return not is_eol(cur); });


     return Selection(first, last);
}

SelectionAndCaptures select_whole_buffer(const Selection& selection)
{
    const Buffer& buffer = selection.first().buffer();
    return Selection(buffer.begin(), buffer.end()-1);
}

SelectionAndCaptures select_next_match(const Selection& selection,
                                       const String& regex)
{
    try
    {
        BufferIterator begin = selection.last();
        BufferIterator end = begin;
        CaptureList captures;

        Regex ex(regex.begin(), regex.end());
        boost::match_results<BufferIterator> matches;

        if (boost::regex_search(begin+1, begin.buffer().end(),
                                matches, ex))
        {
            begin = matches[0].first;
            end   = matches[0].second;
            for (auto& match : matches)
                captures.push_back(String(match.first, match.second));
        }
        else if (boost::regex_search(begin.buffer().begin(), begin+1,
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

        return SelectionAndCaptures(begin, end - 1, std::move(captures));
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

SelectionAndCapturesList select_all_matches(const Selection& selection,
                                            const String& regex)
{
    try
    {
        Regex ex(regex.begin(), regex.end());
        RegexIterator re_it(selection.begin(), selection.end(), ex);
        RegexIterator re_end;

        SelectionAndCapturesList result;
        for (; re_it != re_end; ++re_it)
        {
            BufferIterator begin = (*re_it)[0].first;
            BufferIterator end   = (*re_it)[0].second;

            if (begin == selection.end())
                continue;

            CaptureList captures;
            for (auto& match : *re_it)
                captures.push_back(String(match.first, match.second));

            result.push_back(SelectionAndCaptures(begin,
                                                  begin == end ? end : end-1,
                                                  std::move(captures)));
        }
        return result;
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

SelectionAndCapturesList split_selection(const Selection& selection,
                                         const String& separator_regex)
{
    try
    {
        Regex ex(separator_regex.begin(), separator_regex.end());
        RegexIterator re_it(selection.begin(), selection.end(), ex,
                            boost::regex_constants::match_nosubs);
        RegexIterator re_end;

        SelectionAndCapturesList result;
        BufferIterator begin = selection.begin();
        for (; re_it != re_end; ++re_it)
        {
            BufferIterator end = (*re_it)[0].first;

            result.push_back(Selection(begin, (begin == end) ? end : end-1));
            begin = (*re_it)[0].second;
        }
        result.push_back(Selection(begin, selection.last()));
        return result;
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error(String("regex error: ") + err.what());
    }
}

}
