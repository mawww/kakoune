#ifndef keys_hh_INCLUDED
#define keys_hh_INCLUDED

#include <vector>
#include <string>

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

    Modifiers modifiers;
    char      key;

    Key(Modifiers modifiers, char key)
        : modifiers(modifiers), key(key) {}

    bool operator==(const Key& other) const
    { return modifiers == other.modifiers and key == other.key; }
};

typedef std::vector<Key> KeyList;

KeyList parse_keys(const std::string& str);

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
