#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "array_view.hh"
#include "keys.hh"
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

using KeyMap = const ArrayView<NormalCmdDesc>;
extern KeyMap keymap;

}

#endif // normal_hh_INCLUDED
