#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include "clock.hh"
#include "coord.hh"
#include "constexpr_utils.hh"
#include "enum.hh"
#include "optional.hh"
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

constexpr auto enum_desc(Meta::Type<EolFormat>)
{
    return make_array<EnumDesc<EolFormat>, 2>({
        { EolFormat::Lf, "lf" },
        { EolFormat::Crlf, "crlf" },
    });
}

enum class ByteOrderMark
{
    None,
    Utf8
};

constexpr auto enum_desc(Meta::Type<ByteOrderMark>)
{
    return make_array<EnumDesc<ByteOrderMark>, 2>({
        { ByteOrderMark::None, "none" },
        { ByteOrderMark::Utf8, "utf8" },
    });
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

    BufferIterator() noexcept : m_buffer{nullptr}, m_line{} {}
    BufferIterator(const Buffer& buffer, BufferCoord coord) noexcept;

    bool operator== (const BufferIterator& iterator) const noexcept;
    bool operator!= (const BufferIterator& iterator) const noexcept;
    bool operator<  (const BufferIterator& iterator) const noexcept;
    bool operator<= (const BufferIterator& iterator) const noexcept;
    bool operator>  (const BufferIterator& iterator) const noexcept;
    bool operator>= (const BufferIterator& iterator) const noexcept;

    bool operator== (const BufferCoord& coord) const noexcept;
    bool operator!= (const BufferCoord& coord) const noexcept;

    const char& operator* () const noexcept;
    const char& operator[](size_t n) const noexcept;
    size_t operator- (const BufferIterator& iterator) const;

    BufferIterator operator+ (ByteCount size) const;
    BufferIterator operator- (ByteCount size) const;

    BufferIterator& operator+= (ByteCount size);
    BufferIterator& operator-= (ByteCount size);

    BufferIterator& operator++ ();
    BufferIterator& operator-- ();

    BufferIterator operator++ (int);
    BufferIterator operator-- (int);

    const BufferCoord& coord() const noexcept { return m_coord; }
    explicit operator BufferCoord() const noexcept { return m_coord; }
    using Sentinel = BufferCoord;

private:
    SafePtr<const Buffer> m_buffer;
    BufferCoord m_coord;
    StringView m_line;
};

using BufferLines = Vector<StringDataPtr, MemoryDomain::BufferContent>;

// A Buffer is a in-memory representation of a file
//
// The Buffer class permits to read and mutate this file
// representation. It also manage modifications undo/redo and
// provides tools to deal with the line/column nature of text.
class Buffer final : public SafeCountable, public Scope, private OptionManagerWatcher
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
    friend constexpr bool with_bit_ops(Meta::Type<Flags>) { return true; }

    enum class HistoryId : size_t { First = 0, Invalid = (size_t)-1 };

    Buffer(String name, Flags flags, StringView data = {},
           timespec fs_timestamp = InvalidTime);
    Buffer(const Buffer&) = delete;
    Buffer& operator= (const Buffer&) = delete;
    ~Buffer();

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
    bool           undo(size_t count = 1);
    bool           redo(size_t count = 1);
    bool           move_to(HistoryId id);
    HistoryId      current_history_id() const noexcept { return m_history_id; }
    HistoryId      next_history_id() const noexcept { return (HistoryId)m_history.size(); }

    String         string(BufferCoord begin, BufferCoord end) const;
    StringView     substr(BufferCoord begin, BufferCoord end) const;

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

    BufferIterator begin() const;
    BufferIterator end() const;
    LineCount      line_count() const;

    Optional<BufferCoord> last_modification_coord() const;

    StringView operator[](LineCount line) const
    { return m_lines[line]; }

    const StringDataPtr& line_storage(LineCount line) const
    { return m_lines.get_storage(line); }

    // returns an iterator at given coordinates. clamp line_and_column
    BufferIterator iterator_at(BufferCoord coord) const;

    // returns nearest valid coordinates from given ones
    BufferCoord clamp(BufferCoord coord) const;

    BufferCoord offset_coord(BufferCoord coord, CharCount offset, ColumnCount, bool);
    BufferCoordAndTarget offset_coord(BufferCoordAndTarget coord, LineCount offset, ColumnCount tabstop, bool avoid_eol);

    const String& name() const { return m_name; }
    const String& display_name() const { return m_display_name; }

    // returns true if the buffer is in a different state than
    // the last time it was saved
    bool is_modified() const;

    // notify the buffer that it was saved in the current state
    void notify_saved();

    ValueMap& values() const { return m_values; }

    void run_hook_in_own_context(Hook hook, StringView param,
                                 String client_name = {});

    void reload(StringView data, timespec fs_timestamp = InvalidTime);

    void check_invariant() const;

    struct Change
    {
        enum Type : char { Insert, Erase };
        Type type;
        BufferCoord begin;
        BufferCoord end;
    };
    ConstArrayView<Change> changes_since(size_t timestamp) const;

    String debug_description() const;

    // Methods called by the buffer manager
    void on_registered();
    void on_unregistered();

    void throw_if_read_only() const;
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

    using UndoGroup = Vector<Modification, MemoryDomain::BufferMeta>;

    struct HistoryNode : UseMemoryDomain<MemoryDomain::BufferMeta>
    {
        HistoryNode(HistoryId parent);

        HistoryId parent;
        HistoryId redo_child = HistoryId::Invalid;
        TimePoint timepoint;
        UndoGroup undo_group;
    };

    Vector<HistoryNode> m_history;
    HistoryId           m_history_id = HistoryId::Invalid;
    HistoryId           m_last_save_history_id = HistoryId::Invalid;
    UndoGroup           m_current_undo_group;

          HistoryNode& history_node(HistoryId id)       { return m_history[(size_t)id]; }
    const HistoryNode& history_node(HistoryId id) const { return m_history[(size_t)id]; }
          HistoryNode& current_history_node()           { return m_history[(size_t)m_history_id]; }
    const HistoryNode& current_history_node()     const { return m_history[(size_t)m_history_id]; }

    Vector<Change, MemoryDomain::BufferMeta> m_changes;

    timespec m_fs_timestamp;

    // Values are just data holding by the buffer, they are not part of its
    // observable state
    mutable ValueMap m_values;
};

}

#include "buffer.inl.hh"

#endif // buffer_hh_INCLUDED
