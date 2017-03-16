#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "hash_map.hh"
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

struct NormalCmd
{
    StringView docstring;
    void (*func)(Context& context, NormalParams params);
};

extern const HashMap<Key, NormalCmd> keymap;

}

#endif // normal_hh_INCLUDED
