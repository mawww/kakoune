#ifndef line_and_column_hh_INCLUDED
#define line_and_column_hh_INCLUDED

namespace Kakoune
{

template<typename EffectiveType>
struct LineAndColumn
{
    int line;
    int column;

    LineAndColumn(int line = 0, int column = 0)
        : line(line), column(column) {}

    EffectiveType operator+(const EffectiveType& other) const
    {
        return EffectiveType(line + other.line, column + other.column);
    }

    EffectiveType& operator+=(const EffectiveType& other)
    {
        line   += other.line;
        column += other.column;
        return *static_cast<EffectiveType*>(this);
    }

    EffectiveType operator-(const EffectiveType& other) const
    {
        return EffectiveType(line + other.line, column + other.column);
    }

    EffectiveType& operator-=(const EffectiveType& other)
    {
        line   += other.line;
        column += other.column;
        return *static_cast<EffectiveType*>(this);
    }
};

}

#endif // line_and_column_hh_INCLUDED
