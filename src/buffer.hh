#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include "clock.hh"
#include "coord.hh"
#include "flags.hh"
#include "safe_ptr.hh"
#include "scope.hh"
#include "shared_string.hh"
#include "value.hh"
#include "vector.hh"

#include <ctime>

namespace Kakoune
{

enum class EolFormat
{
    Lf,
    Crlf
};

constexpr Array<EnumDesc<EolFormat>, 2> enum_desc(EolFormat)
{
    return { {
        { EolFormat::Lf, "lf" },
        { EolFormat::Crlf, "crlf" },
    } };
}

enum class ByteOrderMark
{
    None,
    Utf8
};

constexpr Array<EnumDesc<ByteOrderMark>, 2> enum_desc(ByteOrderMark)
{
    return { {
        { ByteOrderMark::None, "none" },
        { ByteOrderMark::Utf8, "utf8" },
    } };
}

class Buffer;

constexpr timespec InvalidTime = { -1, -1 };

// A BufferIterator permits to iterate over the characters of a buffer
class BufferIterator
{
public:
    using value_type = char;
    using difference_type = ssize_t;
    using pointer = const value_type*;
    using reference = const value_type&;
    // computing the distance between two iterator can be
    // costly, so this is not strictly random access
    using iterator_category = std::bidirectional_iterator_tag;

    BufferIterator() : m_buffer(nullptr) {}
    BufferIterator(const Buffer& buffer, BufferCoord coord);

    bool operator== (const BufferIterator& iterator) const;
    bool operator!= (const BufferIterator& iterator) const;
    bool operator<  (const BufferIterator& iterator) const;
    bool operator<= (const BufferIterator& iterator) const;
    bool operator>  (const BufferIterator& iterator) const;
    bool operator>= (const BufferIterator& iterator) const;

    const char& operator* () const;
    const char& operator[](size_t n) const;
    size_t operator- (const BufferIterator& iterator) const;

    BufferIterator operator+ (ByteCount size) const;
    BufferIterator operator- (ByteCount size) const;

    BufferIterator& operator+= (ByteCount size);
    BufferIterator& operator-= (ByteCount size);

    BufferIterator& operator++ ();
    BufferIterator& operator-- ();

    BufferIterator operator++ (int);
    BufferIterator operator-- (int);

    const BufferCoord& coord() const { return m_coord; }

private:
    SafePtr<const Buffer> m_buffer;
    StringView m_line;
    BufferCoord m_coord;
    LineCount m_last_line;
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
        None     = 0,
        File     = 1 << 0,
        New      = 1 << 1,
        Fifo     = 1 << 2,
        NoUndo   = 1 << 3,
        NoHooks  = 1 << 4,
        Debug    = 1 << 5,
        ReadOnly = 1 << 6,
    };

    Buffer(String name, Flags flags, StringView data = {},
           timespec fs_timestamp = InvalidTime);
    Buffer(const Buffer&) = delete;
    Buffer& operator= (const Buffer&) = delete;
    ~Buffer() override;

    Flags flags() const { return m_flags; }
    Flags& flags() { return m_flags; }

    bool set_name(String name);
    void update_display_name();

    BufferCoord insert(BufferCoord pos, StringView content);
    BufferCoord erase(BufferCoord begin, BufferCoord end);
    BufferCoord replace(BufferCoord begin, BufferCoord end, StringView content);

    size_t         timestamp() const;
    timespec       fs_timestamp() const;
    void           set_fs_timestamp(timespec ts);

    void           commit_undo_group();
    bool           undo(size_t count = 1) noexcept;
    bool           redo(size_t count = 1) noexcept;
    bool           move_to(size_t history_id) noexcept;
    size_t         current_history_id() const noexcept;

    String         string(BufferCoord begin, BufferCoord end) const;

    const char&    byte_at(BufferCoord c) const;
    ByteCount      distance(BufferCoord begin, BufferCoord end) const;
    BufferCoord    advance(BufferCoord coord, ByteCount count) const;
    BufferCoord    next(BufferCoord coord) const;
    BufferCoord    prev(BufferCoord coord) const;

    BufferCoord    char_next(BufferCoord coord) const;
    BufferCoord    char_prev(BufferCoord coord) const;

    BufferCoord    back_coord() const;
    BufferCoord    end_coord() const;

    bool           is_valid(BufferCoord c) const;
    bool           is_end(BufferCoord c) const;

    BufferCoord    last_modification_coord() const;

    BufferIterator begin() const;
    BufferIterator end() const;
    LineCount      line_count() const;

    StringView operator[](LineCount line) const
    { return m_lines[line]; }

    const StringDataPtr& line_storage(LineCount line) const
    { return m_lines.get_storage(line); }

    // returns an iterator at given coordinates. clamp line_and_column
    BufferIterator iterator_at(BufferCoord coord) const;

    // returns nearest valid coordinates from given ones
    BufferCoord clamp(BufferCoord coord) const;

    BufferCoord offset_coord(BufferCoord coord, CharCount offset);
    BufferCoordAndTarget offset_coord(BufferCoordAndTarget coord, LineCount offset);

    const String& name() const { return m_name; }
    const String& display_name() const { return m_display_name; }

    // returns true if the buffer is in a different state than
    // the last time it was saved
    bool is_modified() const;

    // notify the buffer that it was saved in the current state
    void notify_saved();

    ValueMap& values() const { return m_values; }

    void run_hook_in_own_context(StringView hook_name, StringView param,
                                 String client_name = "");

    void reload(StringView data, timespec fs_timestamp = InvalidTime);

    void check_invariant() const;

    struct Change
    {
        enum Type : char { Insert, Erase };
        Type type;
        bool at_end;
        BufferCoord begin;
        BufferCoord end;
    };
    ConstArrayView<Change> changes_since(size_t timestamp) const;

    String debug_description() const;

    // Methods called by the buffer manager
    void on_registered();
    void on_unregistered();
private:

    void on_option_changed(const Option& option) override;

    BufferCoord do_insert(BufferCoord pos, StringView content);
    BufferCoord do_erase(BufferCoord begin, BufferCoord end);

    struct Modification;

    void apply_modification(const Modification& modification);
    void revert_modification(const Modification& modification);

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

    String m_name;
    String m_display_name;
    Flags  m_flags;

    using  UndoGroup = Vector<Modification, MemoryDomain::BufferMeta>;

    struct HistoryNode : SafeCountable, UseMemoryDomain<MemoryDomain::BufferMeta>
    {
        HistoryNode(size_t id, HistoryNode* parent);

        size_t id;
        SafePtr<HistoryNode> parent;
        UndoGroup undo_group;
        Vector<std::unique_ptr<HistoryNode>, MemoryDomain::BufferMeta> childs;
        SafePtr<HistoryNode> redo_child;
        TimePoint timepoint;
    };

    size_t                m_next_history_id = 0;
    HistoryNode           m_history;
    SafePtr<HistoryNode>  m_history_cursor;
    SafePtr<HistoryNode>  m_last_save_history_cursor;
    UndoGroup             m_current_undo_group;

    void move_to(HistoryNode* history_node) noexcept;

    template<typename Func> HistoryNode* find_history_node(HistoryNode* node, const Func& func);

    Vector<Change, MemoryDomain::BufferMeta> m_changes;

    timespec m_fs_timestamp;

    // Values are just data holding by the buffer, they are not part of its
    // observable state
    mutable ValueMap m_values;
};

template<> struct WithBitOps<Buffer::Flags> : std::true_type {};

}

#include "buffer.inl.hh"

#endif // buffer_hh_INCLUDED
