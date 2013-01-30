#include "keys.hh"
#include "utils.hh"

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

using KeyAndName = std::pair<String, Codepoint>;
static std::vector<KeyAndName> keynamemap = {
    { "ret", '\r' },
    { "space", ' ' },
    { "esc", Key::Escape },
    { "left", Key::Left },
    { "right", Key::Right },
    { "up", Key::Up },
    { "down", Key::Down}
};

KeyList parse_keys(const String& str)
{
    KeyList result;
    for (ByteCount pos = 0; pos < str.length(); ++pos)
    {
        if (str[pos] == '<')
        {
            ByteCount end_pos = pos;
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
                        keyname = keyname.substr(2_byte);
                    }
                    if (tolower(keyname[0]) == 'a' and keyname[1] == '-')
                    {
                        modifier = Key::Modifiers::Alt;
                        keyname = keyname.substr(2_byte);
                    }
                }
                if (keyname.length() == 1)
                {
                    result.push_back(Key{ modifier, Codepoint(keyname[0]) });
                    pos = end_pos;
                    continue;
                }
                auto it = find_if(keynamemap, [&keyname](const KeyAndName& item)
                                              { return item.first == keyname; });
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

String key_to_str(const Key& key)
{
    bool named = false;
    String res;
    auto it = find_if(keynamemap, [&key](const KeyAndName& item)
                                  { return item.second == key.key; });
    if (it != keynamemap.end())
    {
        named = true;
        res = it->first;
    }
    else
        res = codepoint_to_str(key.key);

    switch (key.modifiers)
    {
    case Key::Modifiers::Control: res = "c-" + res; named = true; break;
    case Key::Modifiers::Alt:     res = "a-" + res; named = true; break;
    default: break;
    }
    if (named)
        res = '<' + res + '>';
    return res;
}

}
