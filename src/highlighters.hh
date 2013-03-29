#ifndef highlighters_hh_INCLUDED
#define highlighters_hh_INCLUDED

#include "highlighter.hh"

#include "color.hh"

namespace Kakoune
{

void register_highlighters();

using LineAndFlag = std::tuple<LineCount, Color, String>;

}

#endif // highlighters_hh_INCLUDED
