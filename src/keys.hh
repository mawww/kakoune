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
};

typedef std::vector<Key> KeyList;

KeyList parse_keys(const std::string& str);

}

#endif // keys_hh_INCLUDED
