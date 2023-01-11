#include "json.hh"

#include "exception.hh"
#include "string_utils.hh"
#include "unit_tests.hh"
#include "utils.hh"

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
            return c == '\\' or c == '"' or (c >= 0 and c <= 0x1F);
        });

        res += StringView{it, next};
        if (next == end)
            break;

        char buf[7] = {'\\', *next, 0};
        if (*next >= 0 and *next <= 0x1F)
            sprintf(buf, "\\u%04x", *next);

        res += buf;
        it = next+1;
    }
    res += '"';
    return res;
}

static bool is_digit(char c) { return c >= '0' and c <= '9'; }

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
                value += StringView{pos, string_end};
                value.back() = *string_end;
                pos = string_end+1;
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
}};

}
