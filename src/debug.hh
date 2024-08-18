#ifndef debug_hh_INCLUDED
#define debug_hh_INCLUDED

#include "array.hh"
#include "enum.hh"

namespace Kakoune
{

class StringView;

enum class DebugFlags
{
    None     = 0,
    Hooks    = 1 << 0,
    Shell    = 1 << 1,
    Profile  = 1 << 2,
    Keys     = 1 << 3,
    Commands = 1 << 4,
};

constexpr bool with_bit_ops(Meta::Type<DebugFlags>) { return true; }

constexpr auto enum_desc(Meta::Type<DebugFlags>)
{
    return make_array<EnumDesc<DebugFlags>>({
        { DebugFlags::Hooks, "hooks" },
        { DebugFlags::Shell, "shell" },
        { DebugFlags::Profile, "profile" },
        { DebugFlags::Keys, "keys" },
        { DebugFlags::Commands, "commands" },
    });
}

void write_to_debug_buffer(StringView str);

}

#endif // debug_hh_INCLUDED
