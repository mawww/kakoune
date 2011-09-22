#include "selectors.hh"

#include <algorithm>

namespace Kakoune
{

static bool is_eol(char c)
{
    return c == '\n';
}

static bool is_blank(char c)
{
    return c == ' ' or c == '\t' or c == '\n';
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
    return not (is_word(c) or is_blank(c));
}

template<typename T>
void skip_while(BufferIterator& it, T condition)
{
    while (not it.is_end() and condition(*it))
        ++it;
}

template<typename T>
void skip_while_reverse(BufferIterator& it, T condition)
{
    while (not it.is_begin() and condition(*it))
        --it;
}


Selection select_to_next_word(const BufferIterator& cursor)
{
    BufferIterator end = cursor;

    if (is_word(*end))
        skip_while(end, is_word);
    else if (is_punctuation(*end))
        skip_while(end, is_punctuation);

    skip_while(end, is_blank);

    return Selection(cursor, end);
}

Selection select_to_next_word_end(const BufferIterator& cursor)
{
    BufferIterator end = cursor;

    skip_while(end, is_blank);

    if (is_word(*end))
        skip_while(end, is_word);
    else if (is_punctuation(*end))
        skip_while(end, is_punctuation);

    return Selection(cursor, end);
}

Selection select_to_previous_word(const BufferIterator& cursor)
{
    BufferIterator end = cursor;

    skip_while_reverse(end, is_blank);
    if (is_word(*end))
        skip_while_reverse(end, is_word);
    else if (is_punctuation(*end))
        skip_while_reverse(end, is_punctuation);

    return Selection(cursor, end);
}

Selection select_line(const BufferIterator& cursor)
{
    BufferIterator begin = cursor;
    while (not begin.is_begin() and *(begin -1) != '\n')
        --begin;

    BufferIterator end = cursor;
    while (not end.is_end() and *end != '\n')
        ++end;
    return Selection(begin, end + 1);
}

Selection move_select(Window& window, const BufferIterator& cursor, const WindowCoord& offset)
{
    WindowCoord cursor_pos = window.line_and_column_at(cursor);
    WindowCoord new_pos = cursor_pos + offset;

    return Selection(cursor, window.iterator_at(new_pos));
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
                return Selection(begin, it+1);

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
                return Selection(begin, it-1);
            --it;
        }
    }
    return Selection(cursor, cursor);
}

}
