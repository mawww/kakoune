#ifndef keys_hh_INCLUDED
#define keys_hh_INCLUDED

#include "coord.hh"
#include "flags.hh"
#include "hash.hh"
#include "meta.hh"
#include "optional.hh"
#include "unicode.hh"
#include "vector.hh"

namespace Kakoune
{

struct Key
{
    enum class Modifiers : int
    {
        None    = 0,
        Control = 1 << 0,
        Alt     = 1 << 1,
        ControlAlt = Control | Alt,

        MousePress   = 1 << 2,
        MouseRelease = 1 << 3,
        MousePos     = 1 << 4,
        MouseWheelDown = 1 << 5,
        MouseWheelUp = 1 << 6,
        MouseEvent = MousePress | MouseRelease | MousePos |
                     MouseWheelDown | MouseWheelUp,

        Resize = 1 << 7,
    };
    enum NamedKey : Codepoint
    {
        // use UTF-16 surrogate pairs range
        Backspace = 0xD800,
        Delete,
        Escape,
        Return,
        Up,
        Down,
        Left,
        Right,
        PageUp,
        PageDown,
        Home,
        End,
        Tab,
        BackTab,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,
        F25,
        F26,
        F27,
        F28,
        F29,
        F30,
        F31,
        F32,
        F33,
        F34,
        F35,
        F36,
        F37,
        F38,
        F39,
        F40,
        F41,
        F42,
        F43,
        F44,
        F45,
        F46,
        F47,
        F48,
        F49,
        F50,
        F51,
        F52,
        F53,
        F54,
        F55,
        F56,
        F57,
        F58,
        F59,
        F60,
        F61,
        F62,
        F63,
        FocusIn,
        FocusOut,
        Invalid,
    };

    Modifiers modifiers = {};
    Codepoint key = {};

    constexpr Key(Modifiers modifiers, Codepoint key)
        : modifiers(modifiers), key(key) {}

    constexpr Key(Codepoint key)
        : modifiers(Modifiers::None), key(key) {}

    constexpr Key() = default;

    constexpr uint64_t val() const { return (uint64_t)modifiers << 32 | key; }

    constexpr bool operator==(Key other) const { return val() == other.val(); }
    constexpr bool operator!=(Key other) const { return val() != other.val(); }
    constexpr bool operator<(Key other) const { return val() < other.val(); }

    constexpr DisplayCoord coord() const { return {(int)((key & 0xFFFF0000) >> 16), (int)(key & 0x0000FFFF)}; }

    Optional<Codepoint> codepoint() const;
};

constexpr bool with_bit_ops(Meta::Type<Key::Modifiers>) { return true; }

using KeyList = Vector<Key, MemoryDomain::Mapping>;

class String;
class StringView;

KeyList parse_keys(StringView str);
String  key_to_str(Key key);

constexpr Key alt(Key key)
{
    return { key.modifiers | Key::Modifiers::Alt, key.key };
}
constexpr Key ctrl(Key key)
{
    return { key.modifiers | Key::Modifiers::Control, key.key };
}
constexpr Key ctrlalt(Key key)
{
    return { key.modifiers | Key::Modifiers::ControlAlt, key.key };
}

constexpr Codepoint encode_coord(DisplayCoord coord) { return (Codepoint)(((int)coord.line << 16) | ((int)coord.column & 0x0000FFFF)); }

constexpr Key resize(DisplayCoord dim) { return { Key::Modifiers::Resize, encode_coord(dim) }; }

constexpr size_t hash_value(const Key& key) { return hash_values(key.modifiers, key.key); }

}

#endif // keys_hh_INCLUDED
