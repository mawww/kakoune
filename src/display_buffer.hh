#ifndef display_buffer_hh_INCLUDED
#define display_buffer_hh_INCLUDED

#include "buffer.hh"
#include "color.hh"
#include "line_and_column.hh"
#include "string.hh"
#include "utf8.hh"

#include <vector>

namespace Kakoune
{

struct DisplayCoord : LineAndColumn<DisplayCoord, LineCount, CharCount>
{
    constexpr DisplayCoord(LineCount line = 0, CharCount column = 0)
        : LineAndColumn(line, column) {}
};

typedef char Attribute;

enum Attributes
{
    Normal = 0,
    Underline = 1,
    Reverse = 2,
    Blink = 4,
    Bold = 8
};

struct DisplayAtom
{
public:
    enum Type { BufferRange, ReplacedBufferRange, Text };

    DisplayAtom(const Buffer& buffer, BufferCoord begin, BufferCoord end)
        : m_type(BufferRange), m_buffer(&buffer), m_begin(begin), m_end(end) {}

    DisplayAtom(String str, ColorPair colors = { Colors::Default, Colors::Default },
                Attribute attribute = Normal)
        : m_type(Text), m_text(std::move(str)), colors(colors), attribute(attribute) {}

    String content() const
    {
        switch (m_type)
        {
            case BufferRange:
               return m_buffer->string(m_begin, m_end);
            case Text:
            case ReplacedBufferRange:
               return m_text;
        }
        kak_assert(false);
        return 0;
    }

    CharCount length() const
    {
        switch (m_type)
        {
            case BufferRange:
               return utf8::distance(m_buffer->iterator_at(m_begin),
                                     m_buffer->iterator_at(m_end));
            case Text:
            case ReplacedBufferRange:
               return m_text.char_length();
        }
        kak_assert(false);
        return 0;
    }

    const BufferCoord& begin() const
    {
        kak_assert(has_buffer_range());
        return m_begin;
    }

    const BufferCoord& end() const
    {
        kak_assert(has_buffer_range());
        return m_end;
    }

    void replace(String text)
    {
        kak_assert(m_type == BufferRange);
        m_type = ReplacedBufferRange;
        m_text = std::move(text);
    }

    bool has_buffer_range() const
    {
        return m_type == BufferRange or m_type == ReplacedBufferRange;
    }

    Type type() const { return m_type; }

    void trim_begin(CharCount count);
    void trim_end(CharCount count);

public:
    ColorPair      colors = {Colors::Default, Colors::Default};
    Attribute      attribute = Normal;

private:
    friend class DisplayLine;

    Type m_type;

    const Buffer* m_buffer = nullptr;
    BufferCoord m_begin;
    BufferCoord m_end;
    String m_text;
};

using BufferRange = std::pair<BufferCoord, BufferCoord>;
using AtomList = std::vector<DisplayAtom>;

class DisplayLine
{
public:
    using iterator = AtomList::iterator;
    using const_iterator = AtomList::const_iterator;
    using value_type = AtomList::value_type;

    DisplayLine() = default;
    DisplayLine(AtomList atoms);
    DisplayLine(String str, ColorPair color)
    { push_back({ std::move(str), color }); }

    iterator begin() { return m_atoms.begin(); }
    iterator end() { return m_atoms.end(); }

    const_iterator begin() const { return m_atoms.begin(); }
    const_iterator end() const { return m_atoms.end(); }

    const AtomList& atoms() const { return m_atoms; }

    CharCount length() const;
    const BufferRange& range() const { return m_range; }

    // Split atom pointed by it at pos, returns an iterator to the first atom
    iterator split(iterator it, BufferCoord pos);

    iterator insert(iterator it, DisplayAtom atom);
    iterator erase(iterator beg, iterator end);
    void     push_back(DisplayAtom atom);

    // remove first_char from the begining of the line, and make sure
    // the line is less that char_count character
    void trim(CharCount first_char, CharCount char_count);

    void     optimize();
private:
    void compute_range();
    BufferRange m_range = { { INT_MAX, INT_MAX }, { INT_MIN, INT_MIN } };
    AtomList  m_atoms;
};

class DisplayBuffer
{
public:
    using LineList = std::vector<DisplayLine>;
    DisplayBuffer() {}

    LineList& lines() { return m_lines; }
    const LineList& lines() const { return m_lines; }

    // returns the smallest BufferRange which contains every DisplayAtoms
    const BufferRange& range() const { return m_range; }
    void optimize();
    void compute_range();

private:
    LineList m_lines;
    BufferRange m_range;
};

}

#endif // display_buffer_hh_INCLUDED
