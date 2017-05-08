#ifndef highlighters_hh_INCLUDED
#define highlighters_hh_INCLUDED

#include "color.hh"
#include "highlighter.hh"

namespace Kakoune
{

void register_highlighters();

struct InclusiveBufferRange{ BufferCoord first, last; };

inline bool operator==(const InclusiveBufferRange& lhs, const InclusiveBufferRange& rhs)
{
    return lhs.first == rhs.first and lhs.last == rhs.last;
}
String option_to_string(InclusiveBufferRange range);
void option_from_string(StringView str, InclusiveBufferRange& opt);

using LineAndFlag = std::tuple<LineCount, String>;
using RangeAndFace = std::tuple<InclusiveBufferRange, String>;

}

#endif // highlighters_hh_INCLUDED
