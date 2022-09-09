#include "keys.hh"

#include "exception.hh"
#include "ranges.hh"
#include "string.hh"
#include "unit_tests.hh"
#include "utf8_iterator.hh"
#include "utils.hh"
#include "string_utils.hh"

namespace Kakoune
{

struct key_parse_error : runtime_error
{
    using runtime_error::runtime_error;
};

static Key canonicalize_ifn(Key key)
{
    if (key.key > 0 and key.key < 27)
    {
        kak_assert(key.modifiers == Key::Modifiers::None);
        key.modifiers = Key::Modifiers::Control;
        key.key = key.key - 1 + 'a';
    }

    if (key.modifiers & Key::Modifiers::Shift)
    {
        if (is_basic_alpha(key.key))
        {
            // Shift + ASCII letters is just the uppercase letter.
            key.modifiers &= ~Key::Modifiers::Shift;
            key.key = to_upper(key.key);
        }
        else if (key.key < 0xD800 || key.key > 0xDFFF)
        {
            // Shift + any other printable character is not allowed.
            throw key_parse_error(format("Shift modifier only works on special keys and lowercase ASCII, not '{}'", key.key));
        }
    }

    return key;
}

Optional<Codepoint> Key::codepoint() const
{
    if (*this == Key::Return)
        return '\n';
    if (*this == Key::Tab)
        return '\t';
    if (*this == Key::Space or *this == shift(Key::Space))
        return ' ';
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
    { "space", Key::Space },
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
    { "ins", Key::Insert },
    { "del", Key::Delete },
    { "plus", '+' },
    { "minus", '-' },
    { "semicolon", ';' },
    { "percent", '%' },
    { "focus_in", Key::FocusIn },
    { "focus_out", Key::FocusOut },
};

KeyList parse_keys(StringView str)
{
    KeyList result;
    using Utf8It = utf8::iterator<const char*>;
    for (Utf8It it{str.begin(), str}, str_end{str.end(), str}; it < str_end; ++it)
    {
        if (*it != '<')
        {
            auto convert = [](Codepoint cp) -> Codepoint {
                switch (cp)
                {
                    case '\n':   return Key::Return;
                    case '\r':   return Key::Return;
                    case '\b':   return Key::Backspace;
                    case '\t':   return Key::Tab;
                    case ' ':    return Key::Space;
                    case '\033': return Key::Escape;
                    default:     return cp;
                }
            };
            result.emplace_back(Key::Modifiers::None, convert(*it));
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
                throw key_parse_error(format("unable to parse modifier in '{}'",
                                                    full_desc));

            switch(to_lower(desc[0_byte]))
            {
                case 'c': modifier |= Key::Modifiers::Control; break;
                case 'a': modifier |= Key::Modifiers::Alt; break;
                case 's': modifier |= Key::Modifiers::Shift; break;
                default:
                    throw key_parse_error(format("unable to parse modifier in '{}'",
                                                        full_desc));
            }
            desc = StringView{dash+1, desc.end()};
        }

        auto name_it = find_if(keynamemap, [&desc](const KeyAndName& item)
                                           { return item.name == desc; });
        if (name_it != std::end(keynamemap))
            result.push_back(canonicalize_ifn({ modifier, name_it->key }));
        else if (desc.char_length() == 1)
            result.push_back(canonicalize_ifn({ modifier, desc[0_char] }));
        else if (desc[0_byte] == 'F' and desc.length() <= 3)
        {
            int val = str_to_int(desc.substr(1_byte));
            if (val >= 1 and val <= 12)
                result.emplace_back(modifier, Key::F1 + (val - 1));
            else
                throw key_parse_error(format("only F1 through F12 are supported, not '{}'", desc));
        }
        else
            throw key_parse_error("unable to parse " +
                                 StringView{it.base(), end_it.base()+1});

        it = end_it;
    }
    return result;
}

StringView to_string(Key::MouseButton button)
{
    switch (button)
    {
        case Key::MouseButton::Left: return "left";
        case Key::MouseButton::Middle: return "middle";
        case Key::MouseButton::Right: return "right";
        default: kak_assert(false); throw logic_error{};
    }
}

Key::MouseButton str_to_button(StringView str)
{
    if (str == "left") return Key::MouseButton::Left;
    if (str == "middle") return Key::MouseButton::Middle;
    if (str == "right") return Key::MouseButton::Right;
    throw runtime_error(format("invalid mouse button name {}", str));
}

String to_string(Key key)
{
    const auto coord = key.coord() + DisplayCoord{1,1};
    switch (Key::Modifiers(key.modifiers & ~Key::Modifiers::MouseButtonMask))
    {
        case Key::Modifiers::MousePos:
            return format("<mouse:move:{}.{}>", coord.line, coord.column);
        case Key::Modifiers::MousePress:
            return format("<mouse:press:{}:{}.{}>", key.mouse_button(), coord.line, coord.column);
        case Key::Modifiers::MouseRelease:
            return format("<mouse:release:{}:{}.{}>", key.mouse_button(), coord.line, coord.column);
        case Key::Modifiers::Scroll:
            return format("<scroll:{}>", static_cast<int>(key.key));
        case Key::Modifiers::Resize:
            return format("<resize:{}.{}>", coord.line, coord.column);
        default: break;
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
    else if (key.key >= Key::F1 and key.key <= Key::F12)
    {
        named = true;
        res = "F" + to_string((int)(key.key - Key::F1 + 1));
    }
    else
        res = String{key.key};

    if (key.modifiers & Key::Modifiers::Shift)   { res = "s-" + res; named = true; }
    if (key.modifiers & Key::Modifiers::Alt)     { res = "a-" + res; named = true; }
    if (key.modifiers & Key::Modifiers::Control) { res = "c-" + res; named = true; }

    if (named)
        res = StringView{'<'} + res + StringView{'>'};
    return res;
}

UnitTest test_keys{[]()
{
    KeyList keys{
         {Key::Space},
         { 'c' },
         {Key::Up},
         alt('j'),
         ctrl('r'),
         shift(Key::Up),
         ctrl('['),
         ctrl('\\'),
         ctrl(']'),
         ctrl('_'),
    };
    String keys_as_str;
    for (auto& key : keys)
        keys_as_str += to_string(key);
    auto parsed_keys = parse_keys(keys_as_str);
    kak_assert(keys == parsed_keys);
    kak_assert(ConstArrayView<Key>{parse_keys("a<c-a-b>c")} ==
               ConstArrayView<Key>{'a', ctrl(alt({'b'})), 'c'});

    kak_assert(parse_keys("x") == KeyList{ {'x'} });
    kak_assert(parse_keys("<x>") == KeyList{ {'x'} });
    kak_assert(parse_keys("<s-x>") == KeyList{ {'X'} });
    kak_assert(parse_keys("<s-X>") == KeyList{ {'X'} });
    kak_assert(parse_keys("<X>") == KeyList{ {'X'} });
    kak_assert(parse_keys("X") == KeyList{ {'X'} });
    kak_assert(parse_keys("<s-up>") == KeyList{ shift({Key::Up}) });
    kak_assert(parse_keys("<s-tab>") == KeyList{ shift({Key::Tab}) });
    kak_assert(parse_keys("\n") == KeyList{ Key::Return });

    kak_assert(to_string(shift({Key::Tab})) == "<s-tab>");

    kak_expect_throw(key_parse_error, parse_keys("<-x>"));
    kak_expect_throw(key_parse_error, parse_keys("<xy-z>"));
    kak_expect_throw(key_parse_error, parse_keys("<x-y>"));
    kak_expect_throw(key_parse_error, parse_keys("<s-/>"));
    kak_expect_throw(key_parse_error, parse_keys("<s-ë>"));
    kak_expect_throw(key_parse_error, parse_keys("<s-lt>"));
    kak_expect_throw(key_parse_error, parse_keys("<f99>"));
    kak_expect_throw(key_parse_error, parse_keys("<backtab>"));
    kak_expect_throw(key_parse_error, parse_keys("<invalidkey>"));
}};

}
