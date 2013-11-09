#include "keys.hh"

#include "utils.hh"

namespace Kakoune
{

Key canonicalize_ifn(Key key)
{
    if (key.key > 0 and key.key < 27)
    {
        kak_assert(key.modifiers == Key::Modifiers::None);
        key.modifiers = Key::Modifiers::Control;
        key.key = key.key - 1 + 'a';
    }
    return key;
}

using KeyAndName = std::pair<String, Codepoint>;
static std::vector<KeyAndName> keynamemap = {
    { "ret", '\r' },
    { "space", ' ' },
    { "tab", '\t' },
    { "lt", '<' },
    { "gt", '>' },
    { "backspace", Key::Backspace},
    { "esc", Key::Escape },
    { "up", Key::Up },
    { "down", Key::Down},
    { "left", Key::Left },
    { "right", Key::Right },
    { "pageup", Key::PageUp },
    { "pagedown", Key::PageDown },
    { "home", Key::Home },
    { "end", Key::End },
    { "backtab", Key::BackTab },
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
                if ((keyname[0] == 'f' or keyname[0] == 'F') and
                    keyname.length() <= 3)
                {

                    int val = 0;
                    for (auto i = 1_byte; i < keyname.length(); ++i)
                    {
                        char c = keyname[i];
                        if (c >= '0' and c <= '9')
                            val = val*10 + c - '0';
                        else
                        {
                            val = -1;
                            break;
                        }
                    }
                    if (val >= 1 and val <= 12)
                    {
                        result.push_back(Key{ modifier, Key::F1 + (val - 1) });
                        pos = end_pos;
                        continue;
                    }
                }
            }
        }
        result.push_back({Key::Modifiers::None, Codepoint(str[pos])});
    }
    return result;
}

String key_to_str(Key key)
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
    else if (key.key >= Key::F1 and key.key < Key::F12)
    {
        named = true;
        res = "F" + to_string((int)(Codepoint)key.key - (int)(Codepoint)Key::F1 + 1);
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
