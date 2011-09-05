#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include <string>
#include <vector>

#include "utils.hh"

namespace Kakoune
{

class Buffer;
typedef int      BufferPos;
typedef int      BufferSize;
typedef char     BufferChar;
typedef std::basic_string<BufferChar> BufferString;

struct BufferCoord : LineAndColumn
{
    BufferCoord(int line = 0, int column = 0)
        : LineAndColumn(line, column) {}
};

class BufferIterator
{
public:
    typedef BufferChar value_type;
    typedef BufferSize difference_type;
    typedef const value_type* pointer;
    typedef const value_type& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    BufferIterator() : m_buffer(NULL), m_position(0) {}
    BufferIterator(const Buffer& buffer, BufferPos position);
    BufferIterator& operator=(const BufferIterator& iterator);

    bool operator== (const BufferIterator& iterator) const;
    bool operator!= (const BufferIterator& iterator) const;
    bool operator<  (const BufferIterator& iterator) const;
    bool operator<= (const BufferIterator& iterator) const;

    BufferChar operator* () const;
    BufferSize operator- (const BufferIterator& iterator) const;

    BufferIterator operator+ (BufferSize size) const;
    BufferIterator operator- (BufferSize size) const;

    BufferIterator& operator+= (BufferSize size);
    BufferIterator& operator-= (BufferSize size);

    BufferIterator& operator++ ();
    BufferIterator& operator-- ();

    bool is_begin() const;
    bool is_end() const;

    const Buffer& buffer() const;

private:
    const Buffer* m_buffer;
    BufferPos     m_position;
    friend class Buffer;
};

class Buffer
{
public:
    Buffer(const std::string& name);

    void           erase(const BufferIterator& begin,
                         const BufferIterator& end);

    void           insert(const BufferIterator& position,
                          const BufferString& string);

    BufferString   string(const BufferIterator& begin,
                          const BufferIterator& end) const;

    BufferIterator begin() const;
    BufferIterator end() const;

    BufferSize     length() const;

    BufferIterator iterator_at(const BufferCoord& line_and_column) const;
    BufferCoord    line_and_column_at(const BufferIterator& iterator) const;

    BufferCoord     clamp(const BufferCoord& line_and_column) const;

    const std::string& name() const { return m_name; }

    const BufferString& content() const { return m_content; }

private:
    BufferChar at(BufferPos position) const;
    friend class BufferIterator;

    std::vector<BufferPos> m_lines;

    void compute_lines();
    BufferPos line_at(const BufferIterator& iterator) const;
    BufferSize line_length(BufferPos line) const;

    BufferString m_content;

    std::string  m_name;
};

}

#endif // buffer_hh_INCLUDED
