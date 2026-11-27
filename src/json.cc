#include "json.hh"

#include "exception.hh"
#include "string_utils.hh"
#include "unit_tests.hh"
#include "utils.hh"
#include "ranges.hh"

#include <algorithm>
#include <cstdio>

namespace Kakoune
{

String to_json(int i) { return to_string(i); }
String to_json(bool b) { return b ? "true" : "false"; }
String to_json(StringView str)
{
    String res;
    res.reserve(str.length() + 4);
    res += '"';
    for (auto it = str.begin(), end = str.end(); it != end; )
    {
        auto next = std::find_if(it, end, [](char c) {
            return c == '\\' or c == '"' or (unsigned char) c <= 0x1F;
        });

        res += StringView{it, next};
        if (next == end)
            break;

        char buf[7] = {'\\', *next, 0};
        if ((unsigned char) *next <= 0x1F)
            format_to(buf, "\\u{:04}", hex(*next));

        res += buf;
        it = next+1;
    }
    res += '"';
    return res;
}

static bool is_digit(char c) { return c >= '0' and c <= '9'; }

// Parses the 4 hex digits at pos, returns {} if they are not all there yet
static Optional<Codepoint> parse_hex4(const char* pos, const char* end)
{
    if (end - pos < 4)
        return {};
    Codepoint res = 0;
    for (char c : StringView{pos, pos+4})
    {
        res *= 16;
        if (is_digit(c))
            res += c - '0';
        else if (c >= 'a' and c <= 'f')
            res += c - 'a' + 10;
        else if (c >= 'A' and c <= 'F')
            res += c - 'A' + 10;
        else
            throw runtime_error("invalid hex digit in unicode escape");
    }
    return res;
}

// Parses a \uXXXX escape, pos pointing after the 'u', and advances pos past it.
// Characters above U+FFFF are encoded as two escapes, a surrogate pair.
static Optional<Codepoint> parse_unicode_escape(const char*& pos, const char* end)
{
    auto cp = parse_hex4(pos, end);
    if (not cp)
        return {};
    if (*cp >= 0xDC00 and *cp <= 0xDFFF)
        throw runtime_error("invalid unicode surrogate pair");
    if (*cp < 0xD800 or *cp > 0xDBFF)
    {
        pos += 4;
        return cp;
    }
    if ((end - pos > 4 and pos[4] != '\\') or (end - pos > 5 and pos[5] != 'u'))
        throw runtime_error("invalid unicode surrogate pair");
    if (end - pos < 10)
        return {};
    auto low = parse_hex4(pos+6, end);
    if (*low < 0xDC00 or *low > 0xDFFF)
        throw runtime_error("invalid unicode surrogate pair");
    pos += 10;
    return 0x10000 + ((*cp - 0xD800) << 10) + (*low - 0xDC00);
}

static constexpr size_t max_parsing_depth = 100;

JsonResult parse_json_impl(const char* pos, const char* end, size_t depth)
{
    if (not skip_while(pos, end, is_blank))
        return {};

    if (depth >= max_parsing_depth)
        throw runtime_error("maximum parsing depth reached");

    if (is_digit(*pos) or *pos == '-')
    {
        auto digit_end = pos + 1;
        skip_while(digit_end, end, is_digit);
        return { Value{str_to_int({pos, digit_end})}, digit_end };
    }
    if (end - pos > 4 and StringView{pos, pos+4} == "true")
        return { Value{true}, pos+4 };
    if (end - pos > 5 and StringView{pos, pos+5} == "false")
        return { Value{false}, pos+5 };
    if (*pos == '"')
    {
        String value;
        bool escaped = false;
        ++pos;
        for (auto string_end = pos; string_end != end; ++string_end)
        {
            if (escaped)
            {
                escaped = false;
                value += StringView{pos, string_end-1};
                pos = string_end+1;
                switch (*string_end)
                {
                    case '"': case '\\': case '/': value.push_back(*string_end); break;
                    case 'b': value.push_back('\b'); break;
                    case 'f': value.push_back('\f'); break;
                    case 'n': value.push_back('\n'); break;
                    case 'r': value.push_back('\r'); break;
                    case 't': value.push_back('\t'); break;
                    case 'u':
                    {
                        auto cp = parse_unicode_escape(pos, end);
                        if (not cp)
                            return {};
                        value += String{*cp};
                        break;
                    }
                    default: throw runtime_error("invalid escape in string");
                }
                string_end = pos-1;
                continue;
            }
            if (*string_end == '\\')
                escaped = true;
            if (*string_end == '"')
            {
                value += StringView{pos, string_end};
                return {std::move(value), string_end+1};
            }
        }
        return {};
    }
    if (*pos == '[')
    {
        JsonArray array;
        if (++pos == end)
            throw runtime_error("unable to parse array");
        if (*pos == ']')
            return {std::move(array), pos+1};

        while (true)
        {
            auto [element, new_pos] = parse_json_impl(pos, end, depth+1);
            if (not element)
                return {};
            pos = new_pos;
            array.push_back(std::move(element));
            if (not skip_while(pos, end, is_blank))
                return {};

            if (*pos == ',')
                ++pos;
            else if (*pos == ']')
                return {std::move(array), pos+1};
            else
                throw runtime_error("unable to parse array, expected ',' or ']'");
        }
    }
    if (*pos == '{')
    {
        if (++pos == end)
            throw runtime_error("unable to parse object");
        JsonObject object;
        if (*pos == '}')
            return {std::move(object), pos+1};

        while (true)
        {
            auto [name_value, name_end] = parse_json_impl(pos, end, depth+1);
            if (not name_value)
                return {};
            pos = name_end;
            String& name = name_value.as<String>();
            if (not skip_while(pos, end, is_blank))
                return {};
            if (*pos++ != ':')
                throw runtime_error("expected :");

            auto [element, element_end] = parse_json_impl(pos, end, depth+1);
            if (not element)
                return {};
            pos = element_end;
            object.insert({ std::move(name), std::move(element) });
            if (not skip_while(pos, end, is_blank))
                return {};

            if (*pos == ',')
                ++pos;
            else if (*pos == '}')
                return {std::move(object), pos+1};
            else
                throw runtime_error("unable to parse object, expected ',' or '}'");
        }
    }
    throw runtime_error("unable to parse json");
}

JsonResult parse_json(const char* pos, const char* end) { return parse_json_impl(pos,          end,        0); }
JsonResult parse_json(StringView json)                  { return parse_json_impl(json.begin(), json.end(), 0); }

UnitTest test_json_parser{[]()
{
    {
        auto value = parse_json(R"({ "jsonrpc": "2.0", "method": "keys", "params": [ "b", "l", "a", "h" ] })").value;
        kak_assert(value);
    }

    {
        auto value = parse_json("[10,20]").value;
        kak_assert(value and value.is_a<JsonArray>());
        kak_assert(value.as<JsonArray>().at(1).as<int>() == 20);
    }

    {
        auto value = parse_json("-1").value;
        kak_assert(value.as<int>() == -1);
    }

    {
        auto value = parse_json("{}").value;
        kak_assert(value and value.is_a<JsonObject>());
        kak_assert(value.as<JsonObject>().empty());
    }

    {
        char big_nested_array[max_parsing_depth*2+2+1] = {};
        for (size_t i = 0; i < max_parsing_depth+1; i++)
        {
            big_nested_array[i] = '[';
            big_nested_array[i+max_parsing_depth+1] = ']';
        }
        kak_expect_throw(runtime_error, parse_json(big_nested_array));
    }

    {
        auto value = parse_json(R"("a\"b\\c\/d\b\f\n\r\te")").value;
        kak_assert(value and value.as<String>() == "a\"b\\c/d\b\f\n\r\te");
    }

    {
        auto value = parse_json(R"("caf\u00e9 \u2026 \ud83d\ude00")").value;
        kak_assert(value and value.as<String>() == "café … 😀");
    }

    {
        auto value = parse_json(to_json(StringView{"\033"})).value;
        kak_assert(value and value.as<String>() == "\033");
    }

    kak_assert(not parse_json(R"("\u20)").value);
    kak_assert(not parse_json(R"("\ud83d\ude)").value);

    kak_expect_throw(runtime_error, parse_json(R"("\u12zz")"));
    kak_expect_throw(runtime_error, parse_json(R"("\ude00")"));
    kak_expect_throw(runtime_error, parse_json(R"("\ud83dabcdef")"));
    kak_expect_throw(runtime_error, parse_json(R"("\ud83d")"));
    kak_expect_throw(runtime_error, parse_json(R"("\ud83d\u0041")"));
    kak_expect_throw(runtime_error, parse_json(R"("\q")"));
}};

UnitTest test_to_json{[]()
{
    kak_assert(to_json(true) == "true");
    kak_assert(to_json(false) == "false");
    kak_assert(to_json(HashMap<String, Vector<int>>{{"foo", {1,2,3}}, {"\033", {3, 4, 5}}}) == R"({"foo": [1, 2, 3],"\u001b": [3, 4, 5]})");
}};

}
