#include "window.hh"

#include <algorithm>

namespace Kakoune
{

Window::Window(const std::shared_ptr<Buffer> buffer)
    : m_buffer(buffer),
      m_position(0, 0),
      m_cursor(0, 0)
{
}

void Window::erase()
{
    if (m_selections.empty())
    {
        BufferIterator cursor = m_buffer->iterator_at(m_cursor);
        m_buffer->erase(cursor, cursor+1);
    }

    for (auto sel = m_selections.begin(); sel != m_selections.end(); ++sel)
    {
        m_buffer->erase(sel->begin, sel->end);
        sel->end = sel->begin;
    }
}

static LineAndColumn measure_string(const Window::String& string)
{
    LineAndColumn result(0, 0);
    for (size_t i = 0; i < string.length(); ++i)
    {
        if (string[i] == '\n')
        {
            ++result.line;
            result.column = 0;
        }
        else
            ++result.column;
    }
    return result;
}

void Window::insert(const String& string)
{
    if (m_selections.empty())
    {
        m_buffer->insert(m_buffer->iterator_at(m_cursor), string);
        move_cursor(measure_string(string));
    }

    for (auto sel = m_selections.begin(); sel != m_selections.end(); ++sel)
    {
        m_buffer->insert(sel->begin, string);
        sel->begin += string.length();
        sel->end += string.length();
    }
}

void Window::append(const String& string)
{
    if (m_selections.empty())
    {
        move_cursor(LineAndColumn(0 , 1));
        insert(string);
    }

    for (auto sel = m_selections.begin(); sel != m_selections.end(); ++sel)
    {
        m_buffer->insert(sel->end, string);
    }
}

void Window::empty_selections()
{
    m_selections.clear();
}

void Window::select(bool append, const Selector& selector)
{
    if (not append or m_selections.empty())
    {
        empty_selections();
        m_selections.push_back(selector(m_buffer->iterator_at(m_cursor)));
    }
    else
    {
        for (auto sel = m_selections.begin(); sel != m_selections.end(); ++sel)
        {
            sel->end = selector(sel->end).end;
        }
    }
    m_cursor = m_buffer->line_and_column_at(m_selections.back().end);
}

void Window::move_cursor(const LineAndColumn& offset)
{
    m_cursor = m_buffer->clamp(LineAndColumn(m_cursor.line + offset.line,
                                             m_cursor.column + offset.column));
}

void Window::update_display_buffer()
{
    m_display_buffer.clear();

    SelectionList sorted_selections = m_selections;
    std::sort(sorted_selections.begin(), sorted_selections.end(),
              [](const Selection& lhs, const Selection& rhs) { return lhs.begin < rhs.begin; });

    BufferIterator current_position = m_buffer->begin();

    for (Selection& sel : sorted_selections)
    {
        if (current_position != sel.begin)
        {
            DisplayAtom atom;
            atom.content = m_buffer->string(current_position, sel.begin);
            m_display_buffer.append(atom);
        }
        if (sel.begin != sel.end)
        {
            DisplayAtom atom;
            atom.content = m_buffer->string(sel.begin, sel.end);
            atom.attribute = UNDERLINE;
            m_display_buffer.append(atom);
        }
        current_position = sel.end;
    }
    if (current_position != m_buffer->end())
    {
        DisplayAtom atom;
        atom.content = m_buffer->string(current_position, m_buffer->end());
        m_display_buffer.append(atom);
    }
}

}
