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

// A BufferIterator permits to iterate over the characters of a buffer
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
    bool is_valid() const;

    const Buffer& buffer() const;

private:
    const Buffer* m_buffer;
    BufferPos     m_position;
    friend class Buffer;
};

// A Modification holds a single atomic modification to Buffer
struct Modification
{
    enum Type { Insert, Erase };

    Type           type;
    BufferIterator position;
    BufferString   content;

    Modification(Type type, BufferIterator position,
                 const BufferString& content)
        : type(type), position(position), content(content) {}

    Modification inverse() const;

    static Modification make_erase(BufferIterator begin, BufferIterator end);
    static Modification make_insert(BufferIterator position,
                                    const BufferString& content);
};

class ModificationListener
{
public:
    virtual void on_modification(const Modification& modification) = 0;
};

// A Buffer is a in-memory representation of a file
//
// The Buffer class permits to read and mutate this file
// representation. It also manage modifications undo/redo and
// provides tools to deal with the line/column nature of text.
class Buffer
{
public:
    enum class Type
    {
        File,
        NewFile,
        Scratch
    };

    Buffer(const std::string& name, Type type,
           const BufferString& initial_content = "\n");
    Buffer(const Buffer&) = delete;
    Buffer(Buffer&&) = delete;
    Buffer& operator= (const Buffer&) = delete;
    ~Buffer();

    void           begin_undo_group();
    void           end_undo_group();

    void           modify(Modification&& modification);

    bool           undo();
    bool           redo();

    BufferString   string(const BufferIterator& begin,
                          const BufferIterator& end) const;

    BufferIterator begin() const;
    BufferIterator end() const;
    BufferSize     length() const;
    BufferSize     line_count() const;

    BufferIterator iterator_at(const BufferCoord& line_and_column) const;
    BufferCoord    line_and_column_at(const BufferIterator& iterator) const;

    // returns nearest valid coordinates from given ones
    BufferCoord    clamp(const BufferCoord& line_and_column) const;

    const std::string& name() const { return m_name; }

    const BufferString& content() const { return m_content; }

    Window* get_or_create_window();
    void delete_window(Window* window);

    std::unique_ptr<Window> create_temporary_window();

    bool is_modified() const;
    Type type() const { return m_type; }
    void notify_saved();

    void register_modification_listener(ModificationListener* listener);
    void unregister_modification_listener(ModificationListener* listener);

    // returns an iterator pointing to the first character of the line
    // iterator is on
    BufferIterator iterator_at_line_begin(const BufferIterator& iterator) const;

    // returns an iterator pointing to the character after the last of the
    // line iterator is on (which is the first of the next line if iterator is
    // not on the last one)
    BufferIterator iterator_at_line_end(const BufferIterator& iterator) const;

private:
    BufferChar at(BufferPos position) const;

    friend class BufferIterator;

    std::vector<BufferPos> m_lines;

    void compute_lines();
    BufferPos line_at(const BufferIterator& iterator) const;
    BufferSize line_length(BufferPos line) const;

    BufferString m_content;

    std::string  m_name;
    const Type   m_type;

    typedef std::vector<Modification> UndoGroup;

    std::vector<UndoGroup>           m_history;
    std::vector<UndoGroup>::iterator m_history_cursor;
    UndoGroup                        m_current_undo_group;

    void apply_modification(const Modification& modification);
    void revert_modification(const Modification& modification);

    std::list<std::unique_ptr<Window>> m_windows;

    size_t m_last_save_undo_index;

    std::vector<ModificationListener*> m_modification_listeners;
};

inline Modification Modification::make_erase(BufferIterator begin,
                                             BufferIterator end)
{
    return Modification(Erase, begin, begin.buffer().string(begin, end));
}

inline Modification Modification::make_insert(BufferIterator position,
                                              const BufferString& content)
{
    return Modification(Insert, position, content);
}

}

#include "buffer_iterator.inl.hh"

#endif // buffer_hh_INCLUDED
