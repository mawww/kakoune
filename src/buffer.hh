#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include <string>
#include <vector>
#include <list>
#include <memory>

#include "line_and_column.hh"

namespace Kakoune
{

class Buffer;
class Window;

typedef int      BufferPos;
typedef int      BufferSize;
typedef char     BufferChar;
typedef std::basic_string<BufferChar> BufferString;

struct BufferCoord : LineAndColumn<BufferCoord>
{
    BufferCoord(int line = 0, int column = 0)
        : LineAndColumn(line, column) {}

    template<typename T>
    explicit BufferCoord(const LineAndColumn<T>& other)
        : LineAndColumn(other.line, other.column) {}
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
    bool operator>  (const BufferIterator& iterator) const;
    bool operator>= (const BufferIterator& iterator) const;

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
    enum class Type
    {
        File,
        Scratch
    };

    Buffer(const std::string& name, Type type,
           const BufferString& initial_content = "");

    void           begin_undo_group();
    void           end_undo_group();

    bool           undo();
    bool           redo();

    void           erase(const BufferIterator& begin,
                         const BufferIterator& end);

    void           insert(const BufferIterator& position,
                          const BufferString& string);

    BufferString   string(const BufferIterator& begin,
                          const BufferIterator& end) const;

    BufferIterator begin() const;
    BufferIterator end() const;
    BufferSize     length() const;
    BufferSize     line_count() const;

    BufferIterator iterator_at(const BufferCoord& line_and_column) const;
    BufferCoord    line_and_column_at(const BufferIterator& iterator) const;

    BufferCoord     clamp(const BufferCoord& line_and_column) const;

    const std::string& name() const { return m_name; }

    const BufferString& content() const { return m_content; }

    Window* get_or_create_window();
    void delete_window(Window* window);

    bool is_modified() const;
    Type type() const { return m_type; }
    void notify_saved();

private:
    BufferChar at(BufferPos position) const;

    void       do_erase(const BufferIterator& begin,
                        const BufferIterator& end);

    void       do_insert(const BufferIterator& position,
                         const BufferString& string);

    friend class BufferIterator;

    std::vector<BufferPos> m_lines;

    void compute_lines();
    BufferPos line_at(const BufferIterator& iterator) const;
    BufferSize line_length(BufferPos line) const;

    BufferString m_content;

    std::string  m_name;
    const Type   m_type;

    struct Modification
    {
        enum Type { Insert, Erase };

        Type           type;
        BufferIterator position;
        BufferString   content;

        Modification(Type type, BufferIterator position, BufferString content)
            : type(type), position(position), content(content) {}

        Modification inverse() const;
    };
    typedef std::vector<Modification> UndoGroup;

    std::vector<UndoGroup>           m_history;
    std::vector<UndoGroup>::iterator m_history_cursor;
    UndoGroup                        m_current_undo_group;

    void replay_modification(const Modification& modification);
    void revert_modification(const Modification& modification);

    void append_modification(Modification&& modification);

    std::list<std::unique_ptr<Window>> m_windows;

    std::vector<UndoGroup>::iterator m_last_save_undo_group;
};

}

#endif // buffer_hh_INCLUDED
