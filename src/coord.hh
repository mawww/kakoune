#ifndef coord_hh_INCLUDED
#define coord_hh_INCLUDED

#include "units.hh"

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
        return EffectiveType(line + other.line, column + other.column);
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
        return EffectiveType(line - other.line, column - other.column);
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

struct CharCoord : LineAndColumn<CharCoord, LineCount, CharCount>
{
    [[gnu::always_inline]]
    constexpr CharCoord(LineCount line = 0, CharCount column = 0)
        : LineAndColumn(line, column) {}
};

}

#endif // coord_hh_INCLUDED
