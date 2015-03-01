#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include "coord.hh"
#include "flags.hh"
#include "safe_ptr.hh"
#include "scope.hh"
#include "shared_string.hh"
#include "value.hh"
#include "vector.hh"

namespace Kakoune
{

class Buffer;

constexpr time_t InvalidTime = 0;

// A BufferIterator permits to iterate over the characters of a buffer
class BufferIterator
{
public:
    using value_type = char;
    using difference_type = size_t;
    using pointer = const value_type*;
    using reference = const value_type&;
    using iterator_category = std::random_access_iterator_tag;

    BufferIterator() : m_buffer(nullptr) {}
    BufferIterator(const Buffer& buffer, ByteCoord coord);

    bool operator== (const BufferIterator& iterator) const;
    bool operator!= (const BufferIterator& iterator) const;
    bool operator<  (const BufferIterator& iterator) const;
    bool operator<= (const BufferIterator& iterator) const;
    bool operator>  (const BufferIterator& iterator) const;
    bool operator>= (const BufferIterator& iterator) const;

    char   operator* () const;
    char   operator[](size_t n) const;
    size_t operator- (const BufferIterator& iterator) const;

    BufferIterator operator+ (ByteCount size) const;
    BufferIterator operator- (ByteCount size) const;

    BufferIterator& operator+= (ByteCount size);
    BufferIterator& operator-= (ByteCount size);

    BufferIterator& operator++ ();
    BufferIterator& operator-- ();

    BufferIterator operator++ (int);
    BufferIterator operator-- (int);

    const ByteCoord& coord() const { return m_coord; }

private:
    SafePtr<const Buffer> m_buffer;
    ByteCoord m_coord;
};

using BufferLines = Vector<StringDataPtr, MemoryDomain::BufferContent>;

// A Buffer is a in-memory representation of a file
//
// The Buffer class permits to read and mutate this file
// representation. It also manage modifications undo/redo and
// provides tools to deal with the line/column nature of text.
class Buffer : public SafeCountable, public OptionManagerWatcher, public Scope
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

    Buffer(String name, Flags flags, BufferLines lines = {},
           time_t fs_timestamp = InvalidTime);
    Buffer(const Buffer&) = delete;
    Buffer& operator= (const Buffer&) = delete;
    ~Buffer();

    Flags flags() const { return m_flags; }
    Flags& flags() { return m_flags; }

    bool set_name(String name);

    BufferIterator insert(const BufferIterator& pos, StringView content);
    BufferIterator erase(BufferIterator begin, BufferIterator end);

    size_t         timestamp() const;
    time_t         fs_timestamp() const;
    void           set_fs_timestamp(time_t ts);

    void           commit_undo_group();
    bool           undo();
    bool           redo();

    String         string(ByteCoord begin, ByteCoord end) const;

    char           byte_at(ByteCoord c) const;
    ByteCount      distance(ByteCoord begin, ByteCoord end) const;
    ByteCoord      advance(ByteCoord coord, ByteCount count) const;
    ByteCoord      next(ByteCoord coord) const;
    ByteCoord      prev(ByteCoord coord) const;

    ByteCoord      char_next(ByteCoord coord) const;
    ByteCoord      char_prev(ByteCoord coord) const;

    ByteCoord      back_coord() const;
    ByteCoord      end_coord() const;

    bool           is_valid(ByteCoord c) const;
    bool           is_end(ByteCoord c) const;

    ByteCoord      last_modification_coord() const;

    BufferIterator begin() const;
    BufferIterator end() const;
    LineCount      line_count() const;

    StringView operator[](LineCount line) const
    { return m_lines[line]; }

    StringDataPtr line_storage(LineCount line) const
    { return m_lines.get_storage(line); }

    // returns an iterator at given coordinates. clamp line_and_column
    BufferIterator iterator_at(ByteCoord coord) const;

    // returns nearest valid coordinates from given ones
    ByteCoord clamp(ByteCoord coord) const;

    ByteCoord offset_coord(ByteCoord coord, CharCount offset);
    ByteCoordAndTarget offset_coord(ByteCoordAndTarget coord, LineCount offset);

    const String& name() const { return m_name; }
    String display_name() const;

    // returns true if the buffer is in a different state than
    // the last time it was saved
    bool is_modified() const;

    // notify the buffer that it was saved in the current state
    void notify_saved();

    ValueMap& values() const { return m_values; }

    void run_hook_in_own_context(const String& hook_name, StringView param);

    void reload(BufferLines lines, time_t fs_timestamp = InvalidTime);

    void check_invariant() const;

    struct Change
    {
        enum Type : char { Insert, Erase };
        Type type;
        bool at_end;
        ByteCoord begin;
        ByteCoord end;
    };
    ArrayView<Change> changes_since(size_t timestamp) const;

    String debug_description() const;
private:

    void on_option_changed(const Option& option) override;

    struct LineList : BufferLines
    {
        [[gnu::always_inline]]
        StringDataPtr& get_storage(LineCount line)
        { return BufferLines::operator[]((int)line); }

        [[gnu::always_inline]]
        const StringDataPtr& get_storage(LineCount line) const
        { return BufferLines::operator[]((int)line); }

        [[gnu::always_inline]]
        StringView operator[](LineCount line) const
        { return get_storage(line)->strview(); }

        StringView front() const { return BufferLines::front()->strview(); }
        StringView back() const { return BufferLines::back()->strview(); }
    };
    LineList m_lines;

    ByteCoord do_insert(ByteCoord pos, StringView content);
    ByteCoord do_erase(ByteCoord begin, ByteCoord end);

    String  m_name;
    Flags   m_flags;

    struct Modification;
    using  UndoGroup = Vector<Modification, MemoryDomain::BufferMeta>;
    friend class UndoGroupOptimizer;

    using History = Vector<UndoGroup, MemoryDomain::BufferMeta>;
    History           m_history;
    History::iterator m_history_cursor;
    UndoGroup         m_current_undo_group;

    void apply_modification(const Modification& modification);
    void revert_modification(const Modification& modification);

    size_t m_last_save_undo_index;

    Vector<Change, MemoryDomain::BufferMeta> m_changes;

    time_t m_fs_timestamp;

    // Values are just data holding by the buffer, so it is part of its
    // observable state
    mutable ValueMap m_values;
};

template<> struct WithBitOps<Buffer::Flags> : std::true_type {};

}

#include "buffer.inl.hh"

#endif // buffer_hh_INCLUDED
