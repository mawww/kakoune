#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "keys.hh"
#include "unordered_map.hh"

#include <functional>

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
    const char* docstring;
    std::function<void (Context& context, NormalParams params)> func;
};

using KeyMap = UnorderedMap<Key, NormalCmdDesc>;
extern KeyMap keymap;

}

#endif // normal_hh_INCLUDED
