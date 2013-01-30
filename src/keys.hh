#ifndef keys_hh_INCLUDED
#define keys_hh_INCLUDED

#include <vector>
#include "unicode.hh"
#include "string.hh"

namespace Kakoune
{

struct Key
{
    enum class Modifiers
    {
        None    = 0,
        Control = 1,
        Alt     = 2,
        ControlAlt = 3
    };
    enum NamedKey : Codepoint
    {
        // use UTF-16 surrogate pairs range
        Backspace = 0xD800,
        Escape,
        Up,
        Down,
        Left,
        Right,
        PageUp,
        PageDown,
        BackTab,
        Invalid,
    };

    Modifiers modifiers;
    Codepoint key;

    constexpr Key(Modifiers modifiers, Codepoint key)
        : modifiers(modifiers), key(key) {}

    constexpr Key(Codepoint key)
        : modifiers(Modifiers::None), key(key) {}

    constexpr bool operator==(const Key& other) const
    { return modifiers == other.modifiers and key == other.key; }

    constexpr bool operator!=(const Key& other) const
    { return modifiers != other.modifiers or key != other.key; }
};

typedef std::vector<Key> KeyList;

KeyList parse_keys(const String& str);
String  key_to_str(const Key& key);

}

namespace std
{

template<>
struct hash<Kakoune::Key> : unary_function<const Kakoune::Key&, size_t>
{
    size_t operator()(const Kakoune::Key& key) const
    {
        return static_cast<size_t>(key.modifiers) * 1024 + key.key;
    }
};

}


#endif // keys_hh_INCLUDED
