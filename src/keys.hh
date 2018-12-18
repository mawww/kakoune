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
        Shift   = 1 << 2,

        MousePressLeft    = 1 << 3,
        MousePressRight   = 1 << 4,
        MouseReleaseLeft  = 1 << 5,
        MouseReleaseRight = 1 << 6,
        MousePos          = 1 << 7,
        MouseWheelDown    = 1 << 8,
        MouseWheelUp      = 1 << 9,
        MouseEvent        = MousePressLeft | MousePressRight |
                            MouseReleaseLeft | MouseReleaseRight |
                            MousePos | MouseWheelDown | MouseWheelUp,

        Resize     = 1 << 10,
        MenuSelect = 1 << 11,
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

constexpr Key shift(Key key)
{
    return { key.modifiers | Key::Modifiers::Shift, key.key };
}
constexpr Key alt(Key key)
{
    return { key.modifiers | Key::Modifiers::Alt, key.key };
}
constexpr Key ctrl(Key key)
{
    return { key.modifiers | Key::Modifiers::Control, key.key };
}

constexpr Codepoint encode_coord(DisplayCoord coord) { return (Codepoint)(((int)coord.line << 16) | ((int)coord.column & 0x0000FFFF)); }

constexpr Key resize(DisplayCoord dim) { return { Key::Modifiers::Resize, encode_coord(dim) }; }

constexpr size_t hash_value(const Key& key) { return hash_values(key.modifiers, key.key); }

}

#endif // keys_hh_INCLUDED
