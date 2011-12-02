#include "selectors.hh"

#include <boost/regex.hpp>
#include <algorithm>

namespace Kakoune
{

static bool is_eol(char c)
{
    return c == '\n';
}

static bool is_blank(char c)
{
    return c == ' ' or c == '\t';
}

static bool is_word(char c)
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

template<bool punctuation_is_not_word = true>
static CharCategories categorize(char c)
{
    if (is_word(c))
        return CharCategories::Word;
    if (is_eol(c))
        return CharCategories::EndOfLine;
    if (is_blank(c))
        return CharCategories::Blank;
    return punctuation_is_not_word ? CharCategories::Punctuation
                                   : CharCategories::Word;
}

template<typename T>
bool skip_while(BufferIterator& it, T condition)
{
    while (not it.is_end() and condition(*it))
        ++it;
    return condition(*it);
}

template<typename T>
bool skip_while_reverse(BufferIterator& it, T condition)
{
    while (not it.is_begin() and condition(*it))
        --it;
    return condition(*it);
}

Selection select_to_next_word(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;
    if (categorize(*begin) != categorize(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);

    BufferIterator end = begin+1;

    if (is_punctuation(*begin))
        skip_while(end, is_punctuation);
    else if (is_word(*begin))
        skip_while(end, is_word);

    bool with_end = skip_while(end, is_blank);

    return Selection(begin, with_end ? end : end-1);
}

Selection select_to_next_word_end(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;
    if (categorize(*begin) != categorize(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);
    BufferIterator end = begin;
    skip_while(end, is_blank);

    bool with_end = false;
    if (is_punctuation(*end))
        with_end = skip_while(end, is_punctuation);
    else if (is_word(*end))
        with_end = skip_while(end, is_word);

    return Selection(begin, with_end ? end : end-1);
}

Selection select_to_previous_word(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;

    if (categorize(*begin) != categorize(*(begin-1)))
        --begin;

    skip_while_reverse(begin, is_eol);
    BufferIterator end = begin;
    skip_while_reverse(end, is_blank);

    bool with_end = false;
    if (is_punctuation(*end))
        with_end = skip_while_reverse(end, is_punctuation);
    else if (is_word(*end))
        with_end = skip_while_reverse(end, is_word);

    return Selection(begin, with_end ? end : end+1);
}

Selection select_to_next_WORD(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;
    if (categorize<false>(*begin) != categorize<false>(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);

    BufferIterator end = begin+1;

    skip_while(end, [] (char c) { return !is_blank(c) and !is_eol(c); });
    bool with_end = skip_while(end, is_blank);

    return Selection(begin, with_end ? end : end-1);
}

Selection select_to_next_WORD_end(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;
    if (categorize<false>(*begin) != categorize<false>(*(begin+1)))
        ++begin;

    skip_while(begin, is_eol);

    BufferIterator end = begin+1;

    skip_while(end, is_blank);
    bool with_end = skip_while(end, [] (char c) { return !is_blank(c)
                                                     and !is_eol(c); });

    return Selection(begin, with_end ? end : end-1);
}

Selection select_to_previous_WORD(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;
    if (categorize<false>(*begin) != categorize<false>(*(begin-1)))
        --begin;

    skip_while_reverse(begin, is_eol);
    BufferIterator end = begin;
    skip_while_reverse(end, is_blank);
    bool with_end = skip_while_reverse(end, [] (char c) { return !is_blank(c)
                                                             and !is_eol(c); });

    return Selection(begin, with_end ? end : end+1);
}

Selection select_line(const BufferIterator& cursor)
{
    BufferIterator first = cursor;
    if (*first == '\n' and not (first + 1).is_end())
        ++first;

    while (not first.is_begin() and *(first - 1) != '\n')
        --first;

    BufferIterator last = first;
    while (not (last + 1).is_end() and *last != '\n')
        ++last;
    return Selection(first, last);
}

Selection select_matching(const BufferIterator& cursor)
{
    std::vector<char> matching_pairs = { '(', ')', '{', '}', '[', ']', '<', '>' };
    BufferIterator it = cursor;
    std::vector<char>::iterator match = matching_pairs.end();
    while (not is_eol(*it))
    {
        match = std::find(matching_pairs.begin(), matching_pairs.end(), *it);
        if (match != matching_pairs.end())
            break;
        ++it;
    }
    if (match == matching_pairs.end())
        return Selection(cursor, cursor);

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
    return Selection(cursor, cursor);
}

Selection select_to(const BufferIterator& cursor, char c, int count, bool inclusive)
{
    BufferIterator end = cursor;
    do
    {
        ++end;
        skip_while(end, [c](char cur) { return not is_eol(cur) and cur != c; });
        if (end.is_end() or is_eol(*end))
            return Selection(cursor, cursor);
    }
    while (--count > 0);

    return Selection(cursor, inclusive ? end : end-1);
}

Selection select_to_reverse(const BufferIterator& cursor, char c, int count, bool inclusive)
{
    BufferIterator end = cursor;
    do
    {
        --end;
        skip_while_reverse(end, [c](char cur) { return not is_eol(cur) and cur != c; });
        if (end.is_begin() or is_eol(*end))
            return Selection(cursor, cursor);
    }
    while (--count > 0);

    return Selection(cursor, inclusive ? end : end+1);
}

Selection select_to_eol(const BufferIterator& cursor)
{
    BufferIterator end = cursor + 1;
    skip_while(end, [](char cur) { return not is_eol(cur); });
    return Selection(cursor, end-1);
}

Selection select_to_eol_reverse(const BufferIterator& cursor)
{
    BufferIterator end = cursor - 1;
    skip_while_reverse(end, [](char cur) { return not is_eol(cur); });
    return Selection(cursor, end.is_begin() ? end : end+1);
}

SelectionList select_whole_lines(const Selection& selection)
{
     BufferIterator first = selection.first();
     BufferIterator last =  selection.last();
     BufferIterator& to_line_start = first <= last ? first : last;
     BufferIterator& to_line_end = first <= last ? last : first;

     --to_line_start;
     skip_while_reverse(to_line_start, [](char cur) { return not is_eol(cur); });
     ++to_line_start;

     skip_while(to_line_end, [](char cur) { return not is_eol(cur); });


     SelectionList result;
     result.push_back(Selection(first, last));
     return result;
}

Selection select_next_match(const BufferIterator& cursor,
                            const std::string& regex)
{
    boost::regex ex(regex);

    BufferIterator begin = cursor;
    BufferIterator end = cursor;
    Selection::CaptureList captures;

    try
    {
        boost::match_results<BufferIterator> matches;

        if (boost::regex_search(cursor, cursor.buffer().end(), matches,
                                ex))
        {
            begin = matches[0].first;
            end   = matches[0].second;
            std::copy(matches.begin(), matches.end(),
                      std::back_inserter(captures));
        }
        else if (boost::regex_search(cursor.buffer().begin(), cursor, matches,
                                     ex))
        {
            begin = matches[0].first;
            end   = matches[0].second;
            std::copy(matches.begin(), matches.end(),
                      std::back_inserter(captures));
        }
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error("regex error");
    }


    if (begin == end)
        ++end;

    return Selection(begin, end - 1, std::move(captures));
}

typedef boost::regex_iterator<BufferIterator> RegexIterator;

SelectionList select_all_matches(const Selection& selection,
                                 const std::string& regex)
{
    boost::regex ex(regex);
    RegexIterator re_it(selection.begin(), selection.end(), ex);
    RegexIterator re_end;

    SelectionList result;
    for (; re_it != re_end; ++re_it)
    {
        BufferIterator begin = (*re_it)[0].first;
        BufferIterator end   = (*re_it)[0].second;

        Selection::CaptureList captures(re_it->begin(), re_it->end());

        result.push_back(Selection(begin, begin == end ? end : end-1,
                                   std::move(captures)));
    }
    return result;
}

SelectionList split_selection(const Selection& selection,
                              const std::string& separator_regex)
{
    boost::regex ex(separator_regex);
    RegexIterator re_it(selection.begin(), selection.end(), ex,
                        boost::regex_constants::match_nosubs);
    RegexIterator re_end;

    SelectionList result;
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

}
