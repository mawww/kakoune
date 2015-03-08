#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "keys.hh"
#include "unordered_map.hh"
#include "string.hh"

namespace Kakoune
{

class Context;

struct NormalParams
{
    int count;
    char reg;
};

struct NormalCmdDesc
{
    Key key;
    StringView docstring;
    void (*func)(Context& context, NormalParams params);
};

using KeyMap = Vector<NormalCmdDesc>;
extern KeyMap keymap;

}

#endif // normal_hh_INCLUDED
