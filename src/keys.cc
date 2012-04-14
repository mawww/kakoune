#include "keys.hh"

#include <unordered_map>

namespace Kakoune
{

static std::unordered_map<String, Character> keynamemap = {
    { "ret", '\n' },
    { "space", ' ' },
    { "esc", 27 }
};

KeyList parse_keys(const String& str)
{
    KeyList result;
    for (size_t pos = 0; pos < str.length(); ++pos)
    {
        if (str[pos] == '<')
        {
            size_t end_pos = pos;
            while (end_pos < str.length() and str[end_pos] != '>')
                ++end_pos;

            if (end_pos < str.length())
            {
                Key::Modifiers modifier = Key::Modifiers::None;

                String keyname = str.substr(pos+1, end_pos - pos - 1);
                if (keyname.length() > 2)
                {
                    if (tolower(keyname[0]) == 'c' and keyname[1] == '-')
                    {
                        modifier = Key::Modifiers::Control;
                        keyname = keyname.substr(2);
                    }
                    if (tolower(keyname[0]) == 'a' and keyname[1] == '-')
                    {
                        modifier = Key::Modifiers::Control;
                        keyname = keyname.substr(2);
                    }
                }
                if (keyname.length() == 1)
                {
                    result.push_back(Key{ modifier, keyname[0] });
                    pos = end_pos;
                    continue;
                }
                auto it = keynamemap.find(keyname);
                if (it != keynamemap.end())
                {
                    result.push_back(Key{ modifier, it->second });
                    pos = end_pos;
                    continue;
                }
            }
        }
        result.push_back({Key::Modifiers::None, str[pos]});
    }
    return result;
}

}
