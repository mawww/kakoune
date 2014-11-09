#include "keys.hh"

#include "utils.hh"
#include "utf8_iterator.hh"

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

using KeyAndName = std::pair<const char*, Codepoint>;
static const KeyAndName keynamemap[] = {
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
    { "del", Key::Delete },
};

KeyList parse_keys(StringView str)
{
    KeyList result;
    using PassPolicy = utf8::InvalidPolicy::Pass;
    using Utf8It = utf8::iterator<const char*, PassPolicy>;
    for (Utf8It it = str.begin(), str_end = str.end(); it < str_end; ++it)
    {
        if (*it == '<')
        {
            Utf8It end_it = it;
            while (end_it < str_end and *end_it != '>')
                ++end_it;

            if (end_it < str_end)
            {
                Key::Modifiers modifier = Key::Modifiers::None;

                StringView keyname{it.base()+1, end_it.base()};
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
                if (keyname.char_length() == 1)
                {
                    result.push_back(Key{ modifier, utf8::codepoint<PassPolicy>(keyname.begin(),keyname.end()) });
                    it = end_it;
                    continue;
                }
                auto name_it = find_if(keynamemap, [&keyname](const KeyAndName& item)
                                                   { return item.first == keyname; });
                if (name_it != end(keynamemap))
                {
                    Key key = canonicalize_ifn(Key{ modifier, name_it->second });
                    result.push_back(key);
                    it = end_it;
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
                        it = end_it;
                        continue;
                    }
                }
            }
        }
        result.push_back({Key::Modifiers::None, *it});
    }
    return result;
}

String key_to_str(Key key)
{
    bool named = false;
    String res;
    auto it = find_if(keynamemap, [&key](const KeyAndName& item)
                                  { return item.second == key.key; });
    if (it != end(keynamemap))
    {
        named = true;
        res = it->first;
    }
    else if (key.key >= Key::F1 and key.key < Key::F12)
    {
        named = true;
        res = "F" + to_string((int)(key.key - Key::F1 + 1));
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
