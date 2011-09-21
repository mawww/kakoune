#include "selectors.hh"

namespace Kakoune
{

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
    return false;
}

Selection select_to_next_word(const BufferIterator& cursor)
{
    BufferIterator end = cursor;
    while (not end.is_end() and is_word(*end))
        ++end;

    while (not end.is_end() and not is_word(*end))
        ++end;

    return Selection(cursor, end);
}

Selection select_to_next_word_end(const BufferIterator& cursor)
{
    BufferIterator end = cursor;
    while (not end.is_end() and not is_word(*end))
        ++end;

    while (not end.is_end() and is_word(*end))
        ++end;

    return Selection(cursor, end);
}

Selection select_to_previous_word(const BufferIterator& cursor)
{
    BufferIterator end = cursor;
    while (not end.is_end() and not is_word(*end))
        --end;

    while (not end.is_end() and is_word(*end))
        --end;

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

}
