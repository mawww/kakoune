#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include "hook_manager.hh"
#include "line_and_column.hh"
#include "option_manager.hh"
#include "string.hh"
#include "units.hh"

#include <vector>
#include <list>
#include <memory>
#include <unordered_set>

namespace Kakoune
{

class Buffer;

struct BufferCoord : LineAndColumn<BufferCoord, LineCount, ByteCount>
{
    constexpr BufferCoord(LineCount line = 0, ByteCount column = 0)
        : LineAndColumn(line, column) {}
};

// A BufferIterator permits to iterate over the characters of a buffer
class BufferIterator
{
public:
    typedef char value_type;
    typedef size_t difference_type;
    typedef const value_type* pointer;
    typedef const value_type& reference;
    typedef std::bidirectional_iterator_tag iterator_category;

    BufferIterator() : m_buffer(nullptr) {}
    BufferIterator(const Buffer& buffer, BufferCoord coord);

    bool operator== (const BufferIterator& iterator) const;
    bool operator!= (const BufferIterator& iterator) const;
    bool operator<  (const BufferIterator& iterator) const;
    bool operator<= (const BufferIterator& iterator) const;
    bool operator>  (const BufferIterator& iterator) const;
    bool operator>= (const BufferIterator& iterator) const;

    char   operator* () const;
    size_t operator- (const BufferIterator& iterator) const;

    BufferIterator operator+ (ByteCount size) const;
    BufferIterator operator- (ByteCount size) const;

    BufferIterator& operator+= (ByteCount size);
    BufferIterator& operator-= (ByteCount size);

    BufferIterator& operator++ ();
    BufferIterator& operator-- ();

    BufferIterator operator++ (int);
    BufferIterator operator-- (int);

    BufferIterator& operator=(const BufferCoord& coord);
    operator const BufferCoord&() const { return m_coord; }

    void clamp(bool avoid_eol);

    bool is_begin() const;
    bool is_end() const;
    bool is_valid() const;

    const Buffer& buffer() const;
    const BufferCoord& coord() const { return m_coord; }
    LineCount line() const { return m_coord.line; }
    ByteCount column() const { return m_coord.column; }
    ByteCount offset() const;

private:
    safe_ptr<const Buffer> m_buffer;
    BufferCoord   m_coord;
};

class BufferChangeListener
{
public:
    virtual void on_insert(const BufferCoord& begin, const BufferCoord& end) = 0;
    virtual void on_erase(const BufferCoord& begin, const BufferCoord& end) = 0;
};

// A Buffer is a in-memory representation of a file
//
// The Buffer class permits to read and mutate this file
// representation. It also manage modifications undo/redo and
// provides tools to deal with the line/column nature of text.
class Buffer : public SafeCountable
{
public:
    enum class Flags
    {
        None = 0,
        File = 1,
        New  = 2,
        Fifo = 4,
        NoUndo = 8,
    };

    Buffer(String name, Flags flags, std::vector<String> lines = { "\n" });
    Buffer(const Buffer&) = delete;
    Buffer& operator= (const Buffer&) = delete;
    ~Buffer();

    Flags flags() const { return m_flags; }
    Flags& flags() { return m_flags; }

    bool set_name(String name);

    void insert(BufferCoord pos, String content);
    void erase(BufferCoord begin, BufferCoord end);

    size_t         timestamp() const { return m_timestamp; }

    void           commit_undo_group();
    bool           undo();
    bool           redo();

    String         string(const BufferCoord& begin,
                          const BufferCoord& end) const;
    ByteCount   offset(const BufferCoord& c) const;
    ByteCount   distance(const BufferCoord& begin, const BufferCoord& end) const;
    BufferCoord advance(BufferCoord coord, ByteCount count) const;
    bool is_valid(const BufferCoord& c) const;
    bool is_end(const BufferCoord& c) const;

    BufferIterator begin() const;
    BufferIterator end() const;
    ByteCount      byte_count() const;
    LineCount      line_count() const;
    ByteCount      line_length(LineCount line) const;
    const String&  line_content(LineCount line) const
    { return m_lines[line].content; }

    // returns an iterator at given coordinates. line_and_column is
    // clamped according to avoid_eol.
    BufferIterator iterator_at(const BufferCoord& line_and_column,
                               bool avoid_eol = false) const;

    // returns nearest valid coordinates from given ones
    // if avoid_eol, clamp to character before eol if line is not empty
    BufferCoord    clamp(const BufferCoord& line_and_column,
                         bool avoid_eol = false) const;

    // returns an iterator pointing to the first character of the line
    // iterator is on
    BufferIterator iterator_at_line_begin(const BufferIterator& iterator) const;
    // the same but taking a line number instead of an iterator
    BufferIterator iterator_at_line_begin(LineCount line) const;

    // returns an iterator pointing to the character after the last of the
    // line iterator is on (which is the first of the next line if iterator is
    // not on the last one)
    BufferIterator iterator_at_line_end(const BufferIterator& iterator) const;
    // the same but taking a line number instead of an iterator
    BufferIterator iterator_at_line_end(LineCount line) const;

    const String& name() const { return m_name; }
    String display_name() const;

    // returns true if the buffer is in a different state than
    // the last time it was saved
    bool is_modified() const;

    // notify the buffer that it was saved in the current state
    void notify_saved();

    OptionManager&       options()       { return m_options; }
    const OptionManager& options() const { return m_options; }
    HookManager&         hooks()         { return m_hooks; }
    const HookManager&   hooks()   const { return m_hooks; }

    std::unordered_set<BufferChangeListener*>& change_listeners() const { return m_change_listeners; }

    void check_invariant() const;
private:
    friend class BufferIterator;

    struct Line
    {
        ByteCount start;
        String    content;

        ByteCount length() const { return content.length(); }
    };
    struct LineList : std::vector<Line>
    {
        Line& operator[](LineCount line)
        { return std::vector<Line>::operator[]((int)line); }

        const Line& operator[](LineCount line) const
        { return std::vector<Line>::operator[]((int)line); }
    };
    LineList m_lines;

    void do_insert(const BufferCoord& pos, const String& content);
    void do_erase(const BufferCoord& begin, const BufferCoord& end);

    String  m_name;
    Flags   m_flags;

    struct Modification;
    typedef std::vector<Modification> UndoGroup;
    friend class UndoGroupOptimizer;

    std::vector<UndoGroup>           m_history;
    std::vector<UndoGroup>::iterator m_history_cursor;
    UndoGroup                        m_current_undo_group;

    void apply_modification(const Modification& modification);
    void revert_modification(const Modification& modification);

    size_t m_last_save_undo_index;
    size_t m_timestamp;

    // this is mutable as adding or removing listeners is not muting the
    // buffer observable state.
    mutable std::unordered_set<BufferChangeListener*> m_change_listeners;

    OptionManager m_options;
    HookManager   m_hooks;
};

constexpr Buffer::Flags operator|(Buffer::Flags lhs, Buffer::Flags rhs)
{
    return (Buffer::Flags)((int) lhs | (int) rhs);
}

inline Buffer::Flags& operator|=(Buffer::Flags& lhs, Buffer::Flags rhs)
{
    (int&) lhs |= (int) rhs;
    return lhs;
}

constexpr bool operator&(Buffer::Flags lhs, Buffer::Flags rhs)
{
    return ((int) lhs & (int) rhs) != 0;
}

inline Buffer::Flags& operator&=(Buffer::Flags& lhs, Buffer::Flags rhs)
{
    (int&) lhs &= (int) rhs;
    return lhs;
}

constexpr Buffer::Flags operator~(Buffer::Flags lhs)
{
    return (Buffer::Flags)(~(int)lhs);
}

struct BufferListenerRegisterFuncs
{
    static void insert(const Buffer& buffer, BufferChangeListener& listener)
    {
        buffer.change_listeners().insert(&listener);
    }
    static void remove(const Buffer& buffer, BufferChangeListener& listener)
    {
        buffer.change_listeners().erase(&listener);
    }
};

class BufferChangeListener_AutoRegister
    : public BufferChangeListener,
      public AutoRegister<BufferChangeListener_AutoRegister,
                          BufferListenerRegisterFuncs, const Buffer>
{
public:
    BufferChangeListener_AutoRegister(const Buffer& buffer)
        : AutoRegister(buffer) {}

    const Buffer& buffer() const { return registry(); }
};

}

#include "buffer_iterator.inl.hh"

#endif // buffer_hh_INCLUDED
