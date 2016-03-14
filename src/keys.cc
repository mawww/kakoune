#include "keys.hh"

#include "containers.hh"
#include "exception.hh"
#include "string.hh"
#include "unit_tests.hh"
#include "utf8_iterator.hh"
#include "utils.hh"

namespace Kakoune
{

static Key canonicalize_ifn(Key key)
{
    if (key.key > 0 and key.key < 27)
    {
        kak_assert(key.modifiers == Key::Modifiers::None);
        key.modifiers = Key::Modifiers::Control;
        key.key = key.key - 1 + 'a';
    }
    return key;
}

Optional<Codepoint> Key::codepoint() const
{
    if (*this == Key::Return)
        return '\n';
    if (*this == Key::Tab)
        return '\t';
    if (*this == Key::Escape)
        return 0x1B;
    if (modifiers == Modifiers::None and key > 27 and
        (key < 0xD800 or key > 0xDFFF)) // avoid surrogates
        return key;
    return {};
}

struct KeyAndName { const char* name; Codepoint key; };
static constexpr KeyAndName keynamemap[] = {
    { "ret", Key::Return },
    { "space", ' ' },
    { "tab", Key::Tab },
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
    { "plus", '+' },
    { "minus", '-' },
};

KeyList parse_keys(StringView str)
{
    KeyList result;
    using Utf8It = utf8::iterator<const char*>;
    for (Utf8It it{str.begin(), str}, str_end{str.end(), str}; it < str_end; ++it)
    {
        if (*it != '<')
        {
            result.emplace_back(Key::Modifiers::None, *it);
            continue;
        }

        Utf8It end_it = std::find(it, str_end, '>');
        if (end_it == str_end)
        {
            result.emplace_back(Key::Modifiers::None, *it);
            continue;
        }

        Key::Modifiers modifier = Key::Modifiers::None;

        StringView full_desc{it.base(), end_it.base()+1};
        StringView desc{it.base()+1, end_it.base()};
        for (auto dash = find(desc, '-'); dash != desc.end(); dash = find(desc, '-'))
        {
            if (dash != desc.begin() + 1)
                throw runtime_error(format("unable to parse modifier in '{}'",
                                           full_desc));

            switch(to_lower(desc[0_byte]))
            {
                case 'c': modifier |= Key::Modifiers::Control; break;
                case 'a': modifier |= Key::Modifiers::Alt; break;
                default:
                    throw runtime_error(format("unable to parse modifier in '{}'",
                                               full_desc));
            }
            desc = StringView{dash+1, desc.end()};
        }

        auto name_it = find_if(keynamemap, [&desc](const KeyAndName& item)
                                           { return item.name == desc; });
        if (name_it != std::end(keynamemap))
            result.push_back(canonicalize_ifn({ modifier, name_it->key }));
        else if (desc.char_length() == 1)
            result.emplace_back(modifier, desc[0_char]);
        else if (to_lower(desc[0_byte]) == 'f' and desc.length() <= 3)
        {
            int val = str_to_int(desc.substr(1_byte));
            if (val >= 1 and val <= 12)
                result.emplace_back(modifier, Key::F1 + (val - 1));
            else
                throw runtime_error("Only F1 through F12 are supported");
        }
        else
            throw runtime_error("Failed to parse " +
                                 StringView{it.base(), end_it.base()+1});

        it = end_it;
    }
    return result;
}

String key_to_str(Key key)
{
    if (auto mouse_event = (key.modifiers & Key::Modifiers::MouseEvent))
    {
        const auto coord = key.coord() + DisplayCoord{1,1};
        switch ((Key::Modifiers)mouse_event)
        {
            case Key::Modifiers::MousePos:
                return format("<mouse:move:{}.{}>", coord.line, coord.column);
            case Key::Modifiers::MousePress:
                return format("<mouse:press:{}.{}>", coord.line, coord.column);
            case Key::Modifiers::MouseRelease:
                return format("<mouse:release:{}.{}>", coord.line, coord.column);
            case Key::Modifiers::MouseWheelDown:
                return "<mouse:wheel_down>";
            case Key::Modifiers::MouseWheelUp:
                return "<mouse:wheel_up>";
            default: kak_assert(false);
        }
    }
    else if (key.modifiers == Key::Modifiers::Resize)
    {
        auto size = key.coord() + DisplayCoord{1,1};
        return format("<resize:{}.{}>", size.line, size.column);
    }

    bool named = false;
    String res;
    auto it = find_if(keynamemap, [&key](const KeyAndName& item)
                                  { return item.key == key.key; });
    if (it != std::end(keynamemap))
    {
        named = true;
        res = it->name;
    }
    else if (key.key >= Key::F1 and key.key < Key::F12)
    {
        named = true;
        res = "F" + to_string((int)(key.key - Key::F1 + 1));
    }
    else
        res = String{key.key};

    switch (key.modifiers)
    {
    case Key::Modifiers::Control:    res = "c-" + res; named = true; break;
    case Key::Modifiers::Alt:        res = "a-" + res; named = true; break;
    case Key::Modifiers::ControlAlt: res = "c-a-" + res; named = true; break;
    default: break;
    }
    if (named)
        res = StringView{'<'} + res + StringView{'>'};
    return res;
}

UnitTest test_keys{[]()
{
    KeyList keys{
         { ' ' },
         { 'c' },
         { Key::Modifiers::Alt, 'j' },
         { Key::Modifiers::Control, 'r' }
    };
    String keys_as_str;
    for (auto& key : keys)
        keys_as_str += key_to_str(key);
    auto parsed_keys = parse_keys(keys_as_str);
    kak_assert(keys == parsed_keys);
    kak_assert(ConstArrayView<Key>{parse_keys("a<c-a-b>c")} ==
               ConstArrayView<Key>{'a', {Key::Modifiers::ControlAlt, 'b'}, 'c'});
}};

}
