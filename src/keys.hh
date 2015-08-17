#ifndef keys_hh_INCLUDED
#define keys_hh_INCLUDED

#include "coord.hh"
#include "flags.hh"
#include "hash.hh"
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
    };
    enum NamedKey : Codepoint
    {
        // use UTF-16 surrogate pairs range
        Backspace = 0xD800,
        Delete,
        Escape,
        Up,
        Down,
        Left,
        Right,
        PageUp,
        PageDown,
        Home,
        End,
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
        FocusIn,
        FocusOut,
        Invalid,
    };

    Modifiers modifiers;
    Codepoint key;

    constexpr Key(Modifiers modifiers, Codepoint key)
        : modifiers(modifiers), key(key) {}

    constexpr Key(Codepoint key)
        : modifiers(Modifiers::None), key(key) {}

    constexpr uint64_t val() const { return (uint64_t)modifiers << 32 | key; }

    constexpr bool operator==(Key other) const { return val() == other.val(); }
    constexpr bool operator!=(Key other) const { return val() != other.val(); }
    constexpr bool operator<(Key other) const { return val() < other.val(); }

    constexpr CharCoord mouse_coord() const { return {(int)((key & 0xFFFF0000) >> 16), (int)(key & 0x0000FFFF)}; }

    Optional<Codepoint> codepoint() const;
};

template<> struct WithBitOps<Key::Modifiers> : std::true_type {};

using KeyList = Vector<Key, MemoryDomain::Mapping>;

class String;
class StringView;

KeyList parse_keys(StringView str);
String  key_to_str(Key key);

constexpr Key alt(Codepoint key) { return { Key::Modifiers::Alt, key }; }
constexpr Key ctrl(Codepoint key) { return { Key::Modifiers::Control, key }; }
constexpr Key ctrlalt(Codepoint key) { return { Key::Modifiers::ControlAlt, key }; }

constexpr Codepoint encode_mouse_coord(CharCoord coord) { return (Codepoint)(((int)coord.line << 16) | ((int)coord.column & 0x0000FFFF)); }

constexpr Key mouse_press(CharCoord pos) { return { Key::Modifiers::MousePress, encode_mouse_coord(pos) }; }
constexpr Key mouse_release(CharCoord pos) { return { Key::Modifiers::MouseRelease, encode_mouse_coord(pos) }; }
constexpr Key mouse_pos(CharCoord pos) { return { Key::Modifiers::MousePos, encode_mouse_coord(pos) }; }
constexpr Key mouse_wheel_down(CharCoord pos) { return { Key::Modifiers::MouseWheelDown, encode_mouse_coord(pos) }; }
constexpr Key mouse_wheel_up(CharCoord pos) { return { Key::Modifiers::MouseWheelUp, encode_mouse_coord(pos) }; }

inline size_t hash_value(const Key& key) { return hash_values(key.modifiers, key.key); }

}

#endif // keys_hh_INCLUDED
