#ifndef selection_hh_INCLUDED
#define selection_hh_INCLUDED

#include "buffer.hh"

#include <limits>

namespace Kakoune
{

using CaptureList = Vector<String, MemoryDomain::Selections>;

constexpr ColumnCount max_column{std::numeric_limits<int>::max()};
constexpr ColumnCount max_non_eol_column{max_column-1};

// A selection is a Selection, associated with a CaptureList
struct Selection
{
    static constexpr MemoryDomain Domain = MemoryDomain::Selections;

    Selection() = default;
    Selection(BufferCoord pos) : Selection(pos,pos) {}
    Selection(BufferCoord anchor, BufferCoordAndTarget cursor,
              CaptureList captures = {})
        : m_anchor{anchor}, m_cursor{cursor},
          m_captures(std::move(captures)) {}

    BufferCoord& anchor() { return m_anchor; }
    BufferCoordAndTarget& cursor() { return m_cursor; }

    const BufferCoord& anchor() const { return m_anchor; }
    const BufferCoordAndTarget& cursor() const { return m_cursor; }

    void set(BufferCoord anchor, BufferCoord cursor)
    {
        m_anchor = anchor;
        m_cursor = cursor;
    }

    void set(BufferCoord coord) { set(coord, coord); }

    CaptureList& captures() { return m_captures; }
    const CaptureList& captures() const { return m_captures; }

    bool operator== (const Selection& other) const
    {
        return m_anchor == other.m_anchor and m_cursor == other.m_cursor;
    }

    // When selections are single char, we want the anchor to be considered min, and cursor max
    const BufferCoord& min() const { return m_anchor <= m_cursor ? m_anchor : m_cursor; }
    const BufferCoord& max() const { return m_anchor <= m_cursor ? m_cursor : m_anchor; }

    BufferCoord& min() { return m_anchor <= m_cursor ? m_anchor : m_cursor; }
    BufferCoord& max() { return m_anchor <= m_cursor ? m_cursor : m_anchor; }

private:
    BufferCoord m_anchor;
    BufferCoordAndTarget m_cursor;

    CaptureList m_captures;
};

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

void replace(Buffer& buffer, Selection& sel, StringView content);
BufferRange insert(Buffer& buffer, Selection& sel, BufferCoord pos, StringView content);

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
    void remove_from(size_t index);

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

    using ApplyFunc = FunctionRef<void (size_t index, Selection& sel)>;
    void for_each(ApplyFunc apply);

    void replace(ConstArrayView<String> strings);

    void erase();

private:
    size_t m_main = 0;
    Vector<Selection> m_selections;

    SafePtr<Buffer> m_buffer;
    size_t m_timestamp;
};

Vector<Selection> compute_modified_ranges(const Buffer& buffer, size_t timestamp);

enum class ColumnType
{
    Byte,
    Codepoint,
    DisplayColumn
};

Selection selection_from_string(ColumnType column_type, const Buffer& buffer, StringView desc, ColumnCount tabstop = -1);
String selection_to_string(ColumnType column_type, const Buffer& buffer, const Selection& selection, ColumnCount tabstop = -1);

String selection_list_to_string(ColumnType column_type, const SelectionList& selections, ColumnCount tabstop = -1);

template<typename StringArray>
SelectionList selection_list_from_strings(Buffer& buffer, ColumnType column_type, StringArray&& descs, size_t timestamp, size_t main, ColumnCount tabstop = -1)
{
    if ((column_type != ColumnType::Byte and timestamp != buffer.timestamp()) or timestamp > buffer.timestamp())
        throw runtime_error{format("invalid timestamp '{}'", timestamp)};

    auto from_string = [&](StringView desc) {
        return selection_from_string(column_type, buffer, desc, tabstop);
    };

    auto sels = descs | transform(from_string) | gather<Vector<Selection>>();
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
