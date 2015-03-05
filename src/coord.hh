#ifndef coord_hh_INCLUDED
#define coord_hh_INCLUDED

#include "units.hh"
#include "hash.hh"

namespace Kakoune
{

template<typename EffectiveType, typename LineType, typename ColumnType>
struct LineAndColumn
{
    LineType   line;
    ColumnType column;

    [[gnu::always_inline]]
    constexpr LineAndColumn(LineType line = 0, ColumnType column = 0)
        : line(line), column(column) {}

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
    constexpr bool operator< (EffectiveType other) const
    {
        return (line != other.line) ? line < other.line
                                    : column < other.column;
    }

    [[gnu::always_inline]]
    constexpr bool operator<= (EffectiveType other) const
    {
        return (line != other.line) ? line < other.line
                                    : column <= other.column;
    }

    [[gnu::always_inline]]
    constexpr bool operator> (EffectiveType other) const
    {
        return (line != other.line) ? line > other.line
                                    : column > other.column;
    }

    [[gnu::always_inline]]
    constexpr bool operator>= (EffectiveType other) const
    {
        return (line != other.line) ? line > other.line
                                    : column >= other.column;
    }

    [[gnu::always_inline]]
    constexpr bool operator== (EffectiveType other) const
    {
        return line == other.line and column == other.column;
    }

    [[gnu::always_inline]]
    constexpr bool operator!= (EffectiveType other) const
    {
        return line != other.line or column != other.column;
    }
};

struct ByteCoord : LineAndColumn<ByteCoord, LineCount, ByteCount>
{
    [[gnu::always_inline]]
    constexpr ByteCoord(LineCount line = 0, ByteCount column = 0)
        : LineAndColumn(line, column) {}
};

inline size_t hash_value(const ByteCoord& val)
{
    return hash_values(val.line, val.column);
}

struct CharCoord : LineAndColumn<CharCoord, LineCount, CharCount>
{
    [[gnu::always_inline]]
    constexpr CharCoord(LineCount line = 0, CharCount column = 0)
        : LineAndColumn(line, column) {}
};

inline size_t hash_value(const CharCoord& val)
{
    return hash_values(val.line, val.column);
}

struct ByteCoordAndTarget : ByteCoord
{
    [[gnu::always_inline]]
    constexpr ByteCoordAndTarget(LineCount line = 0, ByteCount column = 0, CharCount target = -1)
        : ByteCoord(line, column), target(target) {}

    [[gnu::always_inline]]
    constexpr ByteCoordAndTarget(ByteCoord coord, CharCount target = -1)
        : ByteCoord(coord), target(target) {}

    CharCount target;
};

inline size_t hash_value(const ByteCoordAndTarget& val)
{
    return hash_values(val.line, val.column, val.target);
}

}

#endif // coord_hh_INCLUDED
