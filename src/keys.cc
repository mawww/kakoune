#include "keys.hh"

#include <unordered_map>

namespace Kakoune
{

Key canonicalize_ifn(Key key)
{
    if (key.key > 0 and key.key < 27)
    {
        assert(key.modifiers == Key::Modifiers::None);
        key.modifiers = Key::Modifiers::Control;
        key.key = key.key - 1 + 'a';
    }
    return key;
}

static std::unordered_map<String, Codepoint> keynamemap = {
    { "ret", '\r' },
    { "space", ' ' },
    { "esc", Key::Escape }
};

KeyList parse_keys(const String& str)
{
    KeyList result;
    for (CharCount pos = 0; pos < str.length(); ++pos)
    {
        if (str[pos] == '<')
        {
            CharCount end_pos = pos;
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
                        modifier = Key::Modifiers::Alt;
                        keyname = keyname.substr(2);
                    }
                }
                if (keyname.length() == 1)
                {
                    result.push_back(Key{ modifier, Codepoint(keyname[0]) });
                    pos = end_pos;
                    continue;
                }
                auto it = keynamemap.find(keyname);
                if (it != keynamemap.end())
                {
                    Key key = canonicalize_ifn(Key{ modifier, it->second });
                    result.push_back(key);
                    pos = end_pos;
                    continue;
                }
            }
        }
        result.push_back({Key::Modifiers::None, Codepoint(str[pos])});
    }
    return result;
}

}
