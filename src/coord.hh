#ifndef coord_hh_INCLUDED
#define coord_hh_INCLUDED

#include "units.hh"
#include "hash.hh"

namespace Kakoune
{

template<typename EffectiveType, typename LineType, typename ColumnType>
struct LineAndColumn
{
    LineType   line = 0;
    ColumnType column = 0;

    [[gnu::always_inline]]
    constexpr EffectiveType operator+(EffectiveType other) const
    {
        return {line + other.line, column + other.column};
    }

    [[gnu::always_inline]]
    EffectiveType& operator+=(EffectiveType other)
    {
        line   += other.line;
        column += other.column;
        return *static_cast<EffectiveType*>(this);
    }

    [[gnu::always_inline]]
    constexpr EffectiveType operator-(EffectiveType other) const
    {
        return {line - other.line, column - other.column};
    }

    [[gnu::always_inline]]
    EffectiveType& operator-=(EffectiveType other)
    {
        line   -= other.line;
        column -= other.column;
        return *static_cast<EffectiveType*>(this);
    }

    [[gnu::always_inline]]
    constexpr auto operator<=> (EffectiveType other) const
    {
        return (line != other.line) ? line <=> other.line
                                    : column <=> other.column;
    }

    [[gnu::always_inline]]
    constexpr bool operator== (EffectiveType other) const
    {
        return line == other.line and column == other.column;
    }

    friend constexpr size_t hash_value(const EffectiveType& val)
    {
        return hash_values(val.line, val.column);
    }
};

struct BufferCoord : LineAndColumn<BufferCoord, LineCount, ByteCount>
{
    [[gnu::always_inline]]
    constexpr BufferCoord(LineCount line = 0, ByteCount column = 0)
        : LineAndColumn{line, column} {}
};

struct DisplayCoord : LineAndColumn<DisplayCoord, LineCount, ColumnCount>
{
    [[gnu::always_inline]]
    constexpr DisplayCoord(LineCount line = 0, ColumnCount column = 0)
        : LineAndColumn{line, column} {}

    static constexpr const char* option_type_name = "coord";
};

struct BufferCoordAndTarget : BufferCoord
{
    [[gnu::always_inline]]
    constexpr BufferCoordAndTarget(LineCount line = 0, ByteCount column = 0, ColumnCount target = -1)
        : BufferCoord{line, column}, target{target} {}

    [[gnu::always_inline]]
    constexpr BufferCoordAndTarget(BufferCoord coord, ColumnCount target = -1)
        : BufferCoord{coord}, target{target} {}

    ColumnCount target;
};

constexpr size_t hash_value(const BufferCoordAndTarget& val)
{
    return hash_values(val.line, val.column, val.target);
}

}

#endif // coord_hh_INCLUDED
