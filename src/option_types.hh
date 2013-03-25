#ifndef option_types_hh_INCLUDED
#define option_types_hh_INCLUDED

#include "units.hh"
#include "string.hh"
#include "color.hh"

namespace Kakoune
{

struct LineAndFlag
{
    LineCount line;
    Color     color;
    String    flag;

    bool operator==(const LineAndFlag& other) const
    { return line == other.line and color == other.color and flag == other.flag; }

    bool operator!=(const LineAndFlag& other) const
    { return not (*this == other); }
};

String option_to_string(const LineAndFlag& opt);
void option_from_string(const String& str, LineAndFlag& opt);

}

#endif // option_types_hh_INCLUDED
