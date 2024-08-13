#ifndef quoting_hh_INCLUDED
#define quoting_hh_INCLUDED

#include "format.hh"
#include "json.hh"
#include "meta.hh"
#include "string.hh"
#include "string_utils.hh"

namespace Kakoune
{

inline String quote(StringView s)
{
    return format("'{}'", double_up(s, "'"));
}

inline String shell_quote(StringView s)
{
    return format("'{}'", replace(s, "'", R"('\'')"));
}

inline String json_quote(StringView s)
{
    return format("{}", to_json(s));
}

enum class Quoting
{
    Raw,
    Kakoune,
    Shell,
    Json
};

constexpr auto enum_desc(Meta::Type<Quoting>)
{
    return make_array<EnumDesc<Quoting>>({
        { Quoting::Raw, "raw" },
        { Quoting::Kakoune, "kakoune" },
        { Quoting::Shell, "shell" },
        { Quoting::Json, "json" }
    });
}

inline auto quoter(Quoting quoting)
{
    switch (quoting)
    {
        case Quoting::Kakoune: return &quote;
        case Quoting::Shell: return &shell_quote;
        case Quoting::Json: return &json_quote;
        case Quoting::Raw:
        default:
            return +[](StringView s) { return s.str(); };
    }
}

}
#endif // quoting_hh_INCLUDED
