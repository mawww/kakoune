#include "json_ui.hh"

#include "containers.hh"
#include "display_buffer.hh"
#include "exception.hh"
#include "keys.hh"
#include "file.hh"
#include "event_manager.hh"
#include "value.hh"
#include "unit_tests.hh"

#include <utility>

#include <unistd.h>

namespace Kakoune
{

template<typename T>
String to_json(ArrayView<const T> array)
{
    String res;
    for (auto& elem : array)
    {
        if (not res.empty())
            res += ", ";
        res += to_json(elem);
    }
    return "[" + res + "]";
}

template<typename T, MemoryDomain D>
String to_json(const Vector<T, D>& vec) { return to_json(ArrayView<const T>{vec}); }

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

String to_json(Color color)
{
    if (color.color == Kakoune::Color::RGB)
    {
        char buffer[10];
        sprintf(buffer, R"("#%02x%02x%02x")", color.r, color.g, color.b);
        return buffer;
    }
    return to_json(color_to_str(color));
}

String to_json(Attribute attributes)
{
    struct Attr { Attribute attr; StringView name; }
    attrs[] { 
        { Attribute::Exclusive, "exclusive" },
        { Attribute::Underline, "underline" },
        { Attribute::Reverse, "reverse" },
        { Attribute::Blink, "blink" },
        { Attribute::Bold, "bold" },
        { Attribute::Dim, "dim" },
        { Attribute::Italic, "italic" },
    };

    return "[" + join(attrs |
                      filter([=](const Attr& a) { return attributes & a.attr; }) |
                      transform([](const Attr& a) { return to_json(a.name); }),
                      ',', false) + "]";
}

String to_json(Face face)
{
    return format(R"(\{ "fg": {}, "bg": {}, "attributes": {} })",
                  to_json(face.fg), to_json(face.bg), to_json(face.attributes));
}

String to_json(const DisplayAtom& atom)
{
    return format(R"(\{ "face": {}, "contents": {} })", to_json(atom.face), to_json(atom.content()));
}

String to_json(const DisplayLine& line)
{
    return to_json(line.atoms());
}

String to_json(DisplayCoord coord)
{
    return format(R"(\{ "line": {}, "column": {} })", coord.line, coord.column);
}

String to_json(MenuStyle style)
{
    switch (style)
    {
        case MenuStyle::Prompt: return R"("prompt")";
        case MenuStyle::Inline: return R"("inline")";
    }
    return "";
}

String to_json(InfoStyle style)
{
    switch (style)
    {
        case InfoStyle::Prompt: return R"("prompt")";
        case InfoStyle::Inline: return R"("inline")";
        case InfoStyle::InlineAbove: return R"("inlineAbove")";
        case InfoStyle::InlineBelow: return R"("inlineBelow")";
        case InfoStyle::MenuDoc: return R"("menuDoc")";
        case InfoStyle::Modal: return R"("modal")";
    }
    return "";
}

String to_json(CursorMode mode)
{
    switch (mode)
    {
        case CursorMode::Prompt: return R"("prompt")";
        case CursorMode::Buffer: return R"("buffer")";
    }
    return "";
}

String concat()
{
    return "";
}

template<typename First, typename... Args>
String concat(First&& first, Args&&... args)
{
    if (sizeof...(Args) != 0)
        return to_json(first) + ", " + concat(args...);
    return to_json(first);
}

template<typename... Args>
void rpc_call(StringView method, Args&&... args)
{
    auto q = format(R"(\{ "jsonrpc": "2.0", "method": "{}", "params": [{}] }{})",
                    method, concat(std::forward<Args>(args)...), "\n");

    write(1, q);
}

JsonUI::JsonUI()
    : m_stdin_watcher{0, FdEvents::Read,
                      [this](FDWatcher&, FdEvents, EventMode mode) {
        parse_requests(mode);
      }}, m_dimensions{24, 80}
{
    set_signal_handler(SIGINT, SIG_DFL);
}

void JsonUI::draw(const DisplayBuffer& display_buffer,
                  const Face& default_face, const Face& padding_face)
{
    rpc_call("draw", display_buffer.lines(), default_face, padding_face);
}

void JsonUI::draw_status(const DisplayLine& status_line,
                         const DisplayLine& mode_line,
                         const Face& default_face)
{
    rpc_call("draw_status", status_line, mode_line, default_face);
}


void JsonUI::menu_show(ConstArrayView<DisplayLine> items,
                       DisplayCoord anchor, Face fg, Face bg,
                       MenuStyle style)
{
    rpc_call("menu_show", items, anchor, fg, bg, style);
}

void JsonUI::menu_select(int selected)
{
    rpc_call("menu_select", selected);
}

void JsonUI::menu_hide()
{
    rpc_call("menu_hide");
}

void JsonUI::info_show(StringView title, StringView content,
                       DisplayCoord anchor, Face face,
                       InfoStyle style)
{
    rpc_call("info_show", title, content, anchor, face, style);
}

void JsonUI::info_hide()
{
    rpc_call("info_hide");
}

void JsonUI::set_cursor(CursorMode mode, DisplayCoord coord)
{
    rpc_call("set_cursor", mode, coord);
}

void JsonUI::refresh(bool force)
{
    rpc_call("refresh", force);
}

void JsonUI::set_ui_options(const Options& options)
{
    // rpc_call("set_ui_options", options);
}

DisplayCoord JsonUI::dimensions()
{
    return m_dimensions;
}

void JsonUI::set_on_key(OnKeyCallback callback)
{
    m_on_key = std::move(callback);
}

using JsonArray = Vector<Value>;
using JsonObject = HashMap<String, Value>;

static bool is_digit(char c) { return c >= '0' and c <= '9'; }

std::tuple<Value, const char*>
parse_json(const char* pos, const char* end)
{
    using Result = std::tuple<Value, const char*>;

    if (not skip_while(pos, end, is_blank))
        return {};

    if (is_digit(*pos))
    {
        auto digit_end = pos;
        skip_while(digit_end, end, is_digit);
        return Result{ Value{str_to_int({pos, digit_end})}, digit_end };
    }
    if (end - pos > 4 and StringView{pos, pos+4} == "true")
        return Result{ Value{true}, pos+4 };
    if (end - pos > 5 and StringView{pos, pos+5} == "false")
        return Result{ Value{false}, pos+5 };
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
                return Result{std::move(value), string_end+1};
            }
        }
        return {};
    }
    if (*pos == '[')
    {
        JsonArray array;
        if (*++pos == ']')
            return Result{std::move(array), pos+1};

        while (true)
        {
            Value element;
            std::tie(element, pos) = parse_json(pos, end);
            if (not element)
                return {};
            array.push_back(std::move(element));
            if (not skip_while(pos, end, is_blank))
                return {};

            if (*pos == ',')
                ++pos;
            else if (*pos == ']')
                return Result{std::move(array), pos+1};
            else
                throw runtime_error("unable to parse array, expected ',' or ']'");
        }
    }
    if (*pos == '{')
    {
        JsonObject object;
        if (*++pos == '}')
            return Result{std::move(object), pos+1};

        while (true)
        {
            Value name_value;
            std::tie(name_value, pos) = parse_json(pos, end);
            if (not name_value)
                return {};

            String& name = name_value.as<String>();
            if (not skip_while(pos, end, is_blank))
                return {};
            if (*pos++ != ':')
                throw runtime_error("expected :");

            Value element;
            std::tie(element, pos) = parse_json(pos, end);
            if (not element)
                return {};
            object.insert({ std::move(name), std::move(element) });
            if (not skip_while(pos, end, is_blank))
                return {};

            if (*pos == ',')
                ++pos;
            else if (*pos == '}')
                return Result{std::move(object), pos+1};
            else
                throw runtime_error("unable to parse object, expected ',' or '}'");
        }
    }
    throw runtime_error("Could not parse json");
}

std::tuple<Value, const char*>
parse_json(StringView json) { return parse_json(json.begin(), json.end()); }

void JsonUI::eval_json(const Value& json)
{
    if (not json.is_a<JsonObject>())
        throw runtime_error("json request is not an object");

    const JsonObject& object = json.as<JsonObject>();
    auto json_it = object.find("jsonrpc"_sv);
    if (json_it == object.end() or json_it->value.as<String>() != "2.0")
        throw runtime_error("invalid json rpc request");

    auto method_it = object.find("method"_sv);
    if (method_it == object.end())
        throw runtime_error("invalid json rpc request (method missing)");
    StringView method = method_it->value.as<String>();

    auto params_it = object.find("params"_sv);
    if (params_it == object.end())
        throw runtime_error("invalid json rpc request (params missing)");
    const JsonArray& params = params_it->value.as<JsonArray>();

    if (method == "keys")
    {
        for (auto& key_val : params)
        {
            for (auto& key : parse_keys(key_val.as<String>()))
                m_on_key(key);
        }
    }
    else if (method == "resize")
    {
        if (params.size() != 2)
            throw runtime_error("resize expects 2 parameters");

        DisplayCoord dim{params[0].as<int>(), params[1].as<int>()};
        m_dimensions = dim;
        m_on_key(resize(dim));
    }
    else
        throw runtime_error("unknown method");
}

void JsonUI::parse_requests(EventMode mode)
{
    constexpr size_t bufsize = 1024;
    char buf[bufsize];
    while (fd_readable(0))
    {
        ssize_t size = ::read(0, buf, bufsize);
        if (size == -1 or size == 0)
            break;

        m_requests += StringView{buf, buf + size};
    }

    if (not m_on_key)
        return;

    while (not m_requests.empty())
    {
        const char* pos = nullptr;
        try
        {
            Value json;
            std::tie(json, pos) = parse_json(m_requests);
            if (json)
                eval_json(json);
        }
        catch (runtime_error& error)
        {
            write(2, format("error while handling requests '{}': '{}'",
                            m_requests, error.what()));
            // try to salvage request by dropping its first line
            pos = std::min(m_requests.end(), find(m_requests, '\n')+1);
        }
        if (not pos)
            break; // unterminated request ?

        m_requests = String{pos, m_requests.end()};
    }
}

UnitTest test_json_parser{[]()
{
    {
        auto value = std::get<0>(parse_json(R"({ "jsonrpc": "2.0", "method": "keys", "params": [ "b", "l", "a", "h" ] })"));
        kak_assert(value);
    }

    {
        auto value = std::get<0>(parse_json("[10,20]"));
        kak_assert(value and value.is_a<JsonArray>());
        kak_assert(value.as<JsonArray>().at(1).as<int>() == 20);
    }

    {
        auto value = std::get<0>(parse_json("{}"));
        kak_assert(value and value.is_a<JsonObject>());
        kak_assert(value.as<JsonObject>().empty());
    }
}};

}
