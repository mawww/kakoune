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

class DisplayLine;

struct AtomContent
{
public:
    enum Type { BufferRange, ReplacedBufferRange, Text };

    AtomContent(const Buffer& buffer, BufferCoord begin, BufferCoord end)
        : m_type(BufferRange), m_buffer(&buffer), m_begin(begin), m_end(end) {}

    AtomContent(String str)
        : m_type(Text), m_text(std::move(str)) {}

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

private:
    friend class DisplayLine;

    Type m_type;

    const Buffer* m_buffer = nullptr;
    BufferCoord m_begin;
    BufferCoord m_end;
    String m_text;
};

struct DisplayAtom
{
    ColorPair      colors;
    Attribute      attribute;
    AtomContent    content;

    DisplayAtom(AtomContent content,
                ColorPair colors = {Colors::Default, Colors::Default},
                Attribute attribute = Normal)
        : content{std::move(content)}, colors{colors}, attribute{attribute}
    {}
};

class DisplayLine
{
public:
    using AtomList = std::vector<DisplayAtom>;
    using iterator = AtomList::iterator;
    using const_iterator = AtomList::const_iterator;

    explicit DisplayLine(LineCount buffer_line) : m_buffer_line(buffer_line) {}
    DisplayLine(LineCount buffer_line, AtomList atoms)
        : m_buffer_line(buffer_line), m_atoms(std::move(atoms)) {}
    DisplayLine(String str, ColorPair color)
        : m_buffer_line(-1), m_atoms{ { std::move(str), color } } {}

    LineCount buffer_line() const { return m_buffer_line; }

    iterator begin() { return m_atoms.begin(); }
    iterator end() { return m_atoms.end(); }

    const_iterator begin() const { return m_atoms.begin(); }
    const_iterator end() const { return m_atoms.end(); }

    const AtomList& atoms() const { return m_atoms; }

    CharCount length() const;

    // Split atom pointed by it at pos, returns an iterator to the first atom
    iterator split(iterator it, BufferIterator pos);

    iterator insert(iterator it, DisplayAtom atom) { return m_atoms.insert(it, std::move(atom)); }
    void     push_back(DisplayAtom atom) { m_atoms.push_back(std::move(atom)); }

    void     optimize();
private:
    LineCount m_buffer_line;
    AtomList  m_atoms;
};

using BufferRange = std::pair<BufferIterator, BufferIterator>;

class DisplayBuffer
{
public:
    using LineList = std::vector<DisplayLine>;
    DisplayBuffer() {}

    LineList& lines() { return m_lines; }
    const LineList& lines() const { return m_lines; }

    // returns the smallest BufferIterator range which contains every DisplayAtoms
    const BufferRange& range() const { return m_range; }
    void optimize();
    void compute_range();

private:
    LineList m_lines;
    BufferRange m_range;
};

}

#endif // display_buffer_hh_INCLUDED
