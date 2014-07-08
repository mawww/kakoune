#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "keys.hh"

#include <functional>
#include <unordered_map>

namespace Kakoune
{

class Context;

struct NormalCmdDesc
{
    const char* docstring;
    std::function<void (Context& context, int param)> func;
};

using KeyMap = std::unordered_map<Key, NormalCmdDesc>;
extern KeyMap keymap;

}

#endif // normal_hh_INCLUDED
