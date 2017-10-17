#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "optional.hh"
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

Optional<NormalCmd> get_normal_command(Key key);

}

#endif // normal_hh_INCLUDED
