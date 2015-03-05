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
    StringView docstring;
    void (*func)(Context& context, NormalParams params);
};

using KeyMap = UnorderedMap<Key, NormalCmdDesc>;
extern const KeyMap keymap;

}

#endif // normal_hh_INCLUDED
