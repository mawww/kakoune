#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

using CaptureList = Vector<String, MemoryDomain::Selections>;

constexpr ColumnCount max_column{std::numeric_limits<int>::max()};

// A selection is a Selection, associated with a CaptureList
template<typename EffectiveType, typename CoordType, typename CoordAndTargetType>
struct SelectionImpl
{
    static constexpr MemoryDomain Domain = MemoryDomain::Selections;

    SelectionImpl() = default;
    SelectionImpl(CoordType pos) : SelectionImpl(pos,pos) {}
    SelectionImpl(CoordType anchor, CoordAndTargetType cursor,
                  CaptureList captures = {})
        : m_anchor{anchor}, m_cursor{cursor},
          m_captures(std::move(captures)) {}

    CoordType& anchor() { return m_anchor; }
    CoordAndTargetType& cursor() { return m_cursor; }

    const CoordType& anchor() const { return m_anchor; }
    const CoordAndTargetType& cursor() const { return m_cursor; }

    void set(CoordType anchor, CoordType cursor)
    {
        m_anchor = anchor;
        m_cursor = cursor;
    }

    void set(CoordType coord) { set(coord, coord); }

    CaptureList& captures() { return m_captures; }
    const CaptureList& captures() const { return m_captures; }

    bool operator== (const EffectiveType& other) const
    {
        return m_anchor == other.m_anchor and m_cursor == other.m_cursor;
    }

    // When selections are single char, we want the anchor to be considered min, and cursor max
    const CoordType& min() const { return m_anchor <= m_cursor ? m_anchor : m_cursor; }
    const CoordType& max() const { return m_anchor <= m_cursor ? m_cursor : m_anchor; }

    CoordType& min() { return m_anchor <= m_cursor ? m_anchor : m_cursor; }
    CoordType& max() { return m_anchor <= m_cursor ? m_cursor : m_anchor; }

private:
    CoordType m_anchor;
    CoordAndTargetType m_cursor;

    CaptureList m_captures;
};

struct Selection : public SelectionImpl<Selection, BufferCoord, BufferCoordAndTarget>
{
    Selection()
        : SelectionImpl{} {}
    Selection(BufferCoord pos)
        : SelectionImpl{pos} {}
    Selection(BufferCoord anchor, BufferCoordAndTarget cursor, CaptureList captures = {})
        : SelectionImpl{anchor, cursor, captures} {}
};

struct CharSelection : public SelectionImpl<CharSelection, CharCoord, CharCoordAndTarget>
{
    CharSelection()
        : SelectionImpl{} {}
    CharSelection(CharCoord pos)
        : SelectionImpl{pos} {}
    CharSelection(CharCoord anchor, CharCoordAndTarget cursor, CaptureList captures = {})
        : SelectionImpl{anchor, cursor, captures} {}
};

CharSelection char_selection(const Buffer& buffer, const Selection& selection);

inline bool overlaps(const Selection& lhs, const Selection& rhs)
{
    return lhs.min() <= rhs.min() ? lhs.max() >= rhs.min()
                                  : lhs.min() <= rhs.max();
}

void update_selections(Vector<Selection>& selections, size_t& main,
                       const Buffer& buffer, size_t timestamp, bool merge = true);

bool compare_selections(const Selection& lhs, const Selection& rhs);
void sort_selections(Vector<Selection>& selections, size_t& main);
void merge_overlapping_selections(Vector<Selection>& selections, size_t& main);
void clamp_selections(Vector<Selection>& sel, const Buffer& buffer);

enum class InsertMode : unsigned
{
    Insert,
    InsertCursor,
    Append,
    Replace,
    InsertAtLineBegin,
    InsertAtNextLineBegin,
    AppendAtLineEnd,
    OpenLineBelow,
    OpenLineAbove
};

struct SelectionList
{
    static constexpr MemoryDomain Domain = MemoryDomain::Selections;

    SelectionList(Buffer& buffer, Selection s);
    SelectionList(Buffer& buffer, Selection s, size_t timestamp);
    SelectionList(Buffer& buffer, Vector<Selection> s);
    SelectionList(Buffer& buffer, Vector<Selection> s, size_t timestamp);

    void update(bool merge = true);

    void check_invariant() const;

    const Selection& main() const { return (*this)[m_main]; }
    Selection& main() { return (*this)[m_main]; }
    size_t main_index() const { return m_main; }
    void set_main_index(size_t main) { kak_assert(main < size()); m_main = main; }

    void push_back(const Selection& sel) { m_selections.push_back(sel); }
    void push_back(Selection&& sel) { m_selections.push_back(std::move(sel)); }

    Selection& operator[](size_t i) { return m_selections[i]; }
    const Selection& operator[](size_t i) const { return m_selections[i]; }

    void set(Vector<Selection> list, size_t main);
    SelectionList& operator=(Vector<Selection> list)
    {
        const size_t main_index = list.size()-1;
        set(std::move(list), main_index);
        return *this;
    }

    using iterator = Vector<Selection>::iterator;
    iterator begin() { return m_selections.begin(); }
    iterator end() { return m_selections.end(); }

    using const_iterator = Vector<Selection>::const_iterator;
    const_iterator begin() const { return m_selections.begin(); }
    const_iterator end() const { return m_selections.end(); }

    void remove(size_t index);

    const Selection* data() const { return m_selections.data(); }
    size_t size() const { return m_selections.size(); }

    bool operator==(const SelectionList& other) const { return m_buffer == other.m_buffer and m_selections == other.m_selections; }
    bool operator!=(const SelectionList& other) const { return not ((*this) == other); }

    void sort();
    void merge_overlapping();
    void merge_consecutive();
    void sort_and_merge_overlapping();

    Buffer& buffer() const { return *m_buffer; }

    size_t timestamp() const { return m_timestamp; }
    void force_timestamp(size_t timestamp) { m_timestamp = timestamp; }

    void insert(ConstArrayView<String> strings, InsertMode mode,
                Vector<BufferCoord>* out_insert_pos = nullptr);
    void erase();

private:
    size_t m_main = 0;
    Vector<Selection> m_selections;

    SafePtr<Buffer> m_buffer;
    size_t m_timestamp;
};

Vector<Selection> compute_modified_ranges(const Buffer& buffer, size_t timestamp);

String selection_to_string(const Selection& selection);
String char_selection_to_string(const CharSelection& selection);
String selection_list_to_string(const SelectionList& selection);
Selection selection_from_string(StringView desc);

template<class StringArray>
SelectionList selection_list_from_strings(Buffer& buffer, StringArray&& descs, size_t timestamp, size_t main)
{
    if (timestamp > buffer.timestamp())
        throw runtime_error{format("invalid timestamp '{}'", timestamp)};

    auto sels = descs | transform(selection_from_string) | gather<Vector<Selection>>();
    if (sels.empty())
        throw runtime_error{"empty selection description"};
    if (main >= sels.size())
        throw runtime_error{"invalid main selection index"};

    sort_selections(sels, main);
    merge_overlapping_selections(sels, main);
    if (timestamp < buffer.timestamp())
        update_selections(sels, main, buffer, timestamp);
    else
        clamp_selections(sels, buffer);

    SelectionList res{buffer, std::move(sels)};
    res.set_main_index(main);
    return res;
}

}

#endif // selection_hh_INCLUDED
