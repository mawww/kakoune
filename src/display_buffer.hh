#ifndef display_buffer_hh_INCLUDED
#define display_buffer_hh_INCLUDED

#include "face.hh"
#include "coord.hh"
#include "string.hh"

#include <vector>

namespace Kakoune
{

class Buffer;

struct DisplayAtom
{
public:
    enum Type { BufferRange, ReplacedBufferRange, Text };

    DisplayAtom(const Buffer& buffer, ByteCoord begin, ByteCoord end)
        : m_type(BufferRange), m_buffer(&buffer), m_begin(begin), m_end(end)
     { check_invariant(); }

    DisplayAtom(String str, Face face = Face{})
        : m_type(Text), m_text(std::move(str)), face(face)
     { check_invariant(); }

    StringView content() const;
    CharCount length() const;

    const ByteCoord& begin() const
    {
        kak_assert(has_buffer_range());
        return m_begin;
    }

    const ByteCoord& end() const
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

    const Buffer& buffer() const { kak_assert(m_buffer); return *m_buffer; }

    Type type() const { return m_type; }

    void trim_begin(CharCount count);
    void trim_end(CharCount count);

    void check_invariant() const;

    bool operator==(const DisplayAtom& other) const
    {
        return face == other.face and content() == other.content();
    }

public:
    Face face;

private:
    friend class DisplayLine;

    Type m_type;

    const Buffer* m_buffer = nullptr;
    ByteCoord m_begin;
    ByteCoord m_end;
    String m_text;
};

using BufferRange = std::pair<ByteCoord, ByteCoord>;
using AtomList = std::vector<DisplayAtom>;

class DisplayLine
{
public:
    using iterator = AtomList::iterator;
    using const_iterator = AtomList::const_iterator;
    using value_type = AtomList::value_type;

    DisplayLine() = default;
    DisplayLine(AtomList atoms);
    DisplayLine(String str, Face face)
    { push_back({ std::move(str), face }); }

    iterator begin() { return m_atoms.begin(); }
    iterator end() { return m_atoms.end(); }

    const_iterator begin() const { return m_atoms.begin(); }
    const_iterator end() const { return m_atoms.end(); }

    const AtomList& atoms() const { return m_atoms; }

    CharCount length() const;
    const BufferRange& range() const { return m_range; }

    // Split atom pointed by it at pos, returns an iterator to the first atom
    iterator split(iterator it, ByteCoord pos);

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
