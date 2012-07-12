#ifndef display_buffer_hh_INCLUDED
#define display_buffer_hh_INCLUDED

#include <vector>

#include "string.hh"
#include "line_and_column.hh"
#include "buffer.hh"

namespace Kakoune
{

struct DisplayCoord : LineAndColumn<DisplayCoord>
{
    DisplayCoord(int line = 0, int column = 0)
        : LineAndColumn(line, column) {}

    template<typename T>
    explicit DisplayCoord(const LineAndColumn<T>& other)
        : LineAndColumn(other.line, other.column) {}
};

typedef int Attribute;

enum Attributes
{
    Normal = 0,
    Underline = 1,
    Reverse = 2,
    Blink = 4,
    Bold = 8
};

enum class Color
{
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White
};

struct AtomContent
{
public:
    enum Type { BufferRange, ReplacedBufferRange, Text };

    AtomContent(BufferIterator begin, BufferIterator end)
        : m_type(BufferRange),
          m_begin(std::move(begin)),
          m_end(std::move(end)) {}

    AtomContent(String str)
        : m_type(Text), m_text(std::move(str)) {}

    String content() const
    {
        switch (m_type)
        {
            case BufferRange:
               return m_begin.buffer().string(m_begin, m_end);
            case Text:
            case ReplacedBufferRange:
               return m_text;
        }
    }

    BufferIterator& begin()
    {
        assert(has_buffer_range());
        return m_begin;
    }

    BufferIterator& end()
    {
        assert(has_buffer_range());
        return m_end;
    }

    void replace(String text)
    {
        assert(m_type == BufferRange);
        m_type = ReplacedBufferRange;
        m_text = std::move(text);
    }

    bool has_buffer_range() const
    {
        return m_type == BufferRange or m_type == ReplacedBufferRange;
    }

    Type type() const { return m_type; }

private:
    Type m_type;

    BufferIterator m_begin;
    BufferIterator m_end;
    String m_text;
};

struct DisplayAtom
{
    Color          fg_color;
    Color          bg_color;
    Attribute      attribute;

    AtomContent    content;

    DisplayAtom(AtomContent content)
       : content(std::move(content)), attribute(Normal),
         fg_color(Color::Default), bg_color(Color::Default) {}
};

class DisplayLine
{
public:
    using AtomList = std::vector<DisplayAtom>;
    using iterator = AtomList::iterator;
    using const_iterator = AtomList::const_iterator;

    explicit DisplayLine(size_t buffer_line) : m_buffer_line(buffer_line) {}

    size_t buffer_line() const { return m_buffer_line; }

    iterator begin() { return m_atoms.begin(); }
    iterator end() { return m_atoms.end(); }

    const_iterator begin() const { return m_atoms.begin(); }
    const_iterator end() const { return m_atoms.end(); }

    // Split atom pointed by it at pos, returns an iterator to the first atom
    iterator split(iterator it, BufferIterator pos);

    iterator insert(iterator it, DisplayAtom atom) { return m_atoms.insert(it, std::move(atom)); }
    void     push_back(DisplayAtom atom) { m_atoms.push_back(std::move(atom)); }

private:
    size_t   m_buffer_line;
    AtomList m_atoms;
};

class DisplayBuffer
{
public:
    using LineList = std::list<DisplayLine>;
    DisplayBuffer() {}

    LineList& lines() { return m_lines; }
    const LineList& lines() const { return m_lines; }
private:
    LineList m_lines;
};

}

#endif // display_buffer_hh_INCLUDED
