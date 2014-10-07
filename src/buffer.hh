#ifndef buffer_hh_INCLUDED
#define buffer_hh_INCLUDED

#include "coord.hh"
#include "hook_manager.hh"
#include "option_manager.hh"
#include "keymap_manager.hh"
#include "safe_ptr.hh"
#include "interned_string.hh"
#include "value.hh"

#include <vector>
#include <list>
#include <memory>
#include <unordered_set>

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
    safe_ptr<const Buffer> m_buffer;
    ByteCoord m_coord;
};

// A Buffer is a in-memory representation of a file
//
// The Buffer class permits to read and mutate this file
// representation. It also manage modifications undo/redo and
// provides tools to deal with the line/column nature of text.
class Buffer : public SafeCountable, public OptionManagerWatcher
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

    Buffer(String name, Flags flags, std::vector<String> lines = { "\n" },
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

    const InternedString& operator[](LineCount line) const
    { return m_lines[line]; }

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

    OptionManager&       options()       { return m_options; }
    const OptionManager& options() const { return m_options; }
    HookManager&         hooks()         { return m_hooks; }
    const HookManager&   hooks()   const { return m_hooks; }
    KeymapManager&       keymaps()       { return m_keymaps; }
    const KeymapManager& keymaps() const { return m_keymaps; }

    ValueMap& values() const { return m_values; }

    void run_hook_in_own_context(const String& hook_name, const String& param);

    void reload(std::vector<String> lines, time_t fs_timestamp = InvalidTime);

    void check_invariant() const;

    struct Change
    {
        enum Type { Insert, Erase };
        Type type;
        ByteCoord begin;
        ByteCoord end;
        bool at_end;
    };
    memoryview<Change> changes_since(size_t timestamp) const;

    String debug_description() const;
private:

    void on_option_changed(const Option& option) override;

    struct LineList : std::vector<InternedString>
    {
        [[gnu::always_inline]]
        InternedString& operator[](LineCount line)
        { return std::vector<InternedString>::operator[]((int)line); }

        [[gnu::always_inline]]
        const InternedString& operator[](LineCount line) const
        { return std::vector<InternedString>::operator[]((int)line); }
    };
    LineList m_lines;

    ByteCoord do_insert(ByteCoord pos, StringView content);
    ByteCoord do_erase(ByteCoord begin, ByteCoord end);

    String  m_name;
    Flags   m_flags;

    struct Modification;
    using  UndoGroup = std::vector<Modification>;
    friend class UndoGroupOptimizer;

    std::vector<UndoGroup>           m_history;
    std::vector<UndoGroup>::iterator m_history_cursor;
    UndoGroup                        m_current_undo_group;

    void apply_modification(const Modification& modification);
    void revert_modification(const Modification& modification);

    size_t m_last_save_undo_index;

    std::vector<Change> m_changes;

    time_t m_fs_timestamp;

    OptionManager m_options;
    HookManager   m_hooks;
    KeymapManager m_keymaps;

    // Values are just data holding by the buffer, so it is part of its
    // observable state
    mutable ValueMap m_values;

    friend constexpr Flags operator|(Flags lhs, Flags rhs)
    {
        return (Flags)((int) lhs | (int) rhs);
    }

    friend Flags& operator|=(Flags& lhs, Flags rhs)
    {
        (int&) lhs |= (int) rhs;
        return lhs;
    }

    friend constexpr bool operator&(Flags lhs, Flags rhs)
    {
        return ((int) lhs & (int) rhs) != 0;
    }

    friend Flags& operator&=(Flags& lhs, Flags rhs)
    {
        (int&) lhs &= (int) rhs;
        return lhs;
    }

    friend constexpr Flags operator~(Flags lhs)
    {
        return (Flags)(~(int)lhs);
    }
};

}

#include "buffer.inl.hh"

#endif // buffer_hh_INCLUDED
