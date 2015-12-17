#ifndef highlighters_hh_INCLUDED
#define highlighters_hh_INCLUDED

#include "color.hh"
#include "highlighter.hh"

namespace Kakoune
{

void register_highlighters();

using LineAndFlag = std::tuple<LineCount, String>;
using RangeAndFace = std::tuple<BufferRange, String>;

}

#endif // highlighters_hh_INCLUDED
