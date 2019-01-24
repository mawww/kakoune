#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "optional.hh"
#include "keys.hh"
#include "keymap_manager.hh"
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
    StringView docstring = {};
    void (*func)(Context& context, NormalParams params) = nullptr;
};

Optional<NormalCmd> get_normal_command(Key key);

struct KeyInfo
{
    ConstArrayView<Key> keys;
    StringView docstring;
};

String build_autoinfo_for_mapping(Context& context, KeymapMode mode,
                                  ConstArrayView<KeyInfo> built_ins);

}

#endif // normal_hh_INCLUDED
