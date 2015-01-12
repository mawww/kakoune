#ifndef keys_hh_INCLUDED
#define keys_hh_INCLUDED

#include "unicode.hh"
#include "flags.hh"
#include "hash.hh"
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
        ControlAlt = Control | Alt
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
        Invalid,
    };

    Modifiers modifiers;
    Codepoint key;

    constexpr Key(Modifiers modifiers, Codepoint key)
        : modifiers(modifiers), key(key) {}

    constexpr Key(Codepoint key)
        : modifiers(Modifiers::None), key(key) {}

    constexpr bool operator==(Key other) const
    { return modifiers == other.modifiers and key == other.key; }

    constexpr bool operator!=(Key other) const
    { return modifiers != other.modifiers or key != other.key; }
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

inline size_t hash_value(const Key& key) { return hash_values(key.modifiers, key.key); }

}

#endif // keys_hh_INCLUDED
