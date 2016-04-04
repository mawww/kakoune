#include "json_ui.hh"

#include "display_buffer.hh"
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
    struct { Attribute attr; StringView name; }
    attrs[] { 
        { Attribute::Exclusive, "exclusive" },
        { Attribute::Underline, "underline" },
        { Attribute::Reverse, "reverse" },
        { Attribute::Blink, "blink" },
        { Attribute::Bold, "bold" },
        { Attribute::Dim, "dim" },
        { Attribute::Italic, "italic" },
    };

    String res;
    for (auto& attr : attrs)
    {
        if (not (attributes & attr.attr))
            continue;

        if (not res.empty())
            res += ", ";
        res += to_json(attr.name);
    }
    return "[" + res + "]";
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

String to_json(CharCoord coord)
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

    write_stdout(q);
}

JsonUI::JsonUI()
    : m_stdin_watcher{0, [this](FDWatcher&, EventMode mode) {
        parse_requests(mode);
      }}, m_dimensions{24, 80}
{
    set_signal_handler(SIGINT, SIG_DFL);
}

void JsonUI::draw(const DisplayBuffer& display_buffer,
                  const Face& default_face)
{
    rpc_call("draw", display_buffer.lines(), default_face);
}

void JsonUI::draw_status(const DisplayLine& status_line,
                         const DisplayLine& mode_line,
                         const Face& default_face)
{
    rpc_call("draw_status", status_line, mode_line, default_face);
}

bool JsonUI::is_key_available()
{
    return not m_pending_keys.empty();
}

Key JsonUI::get_key()
{
    kak_assert(not m_pending_keys.empty());
    Key key = m_pending_keys.front();
    m_pending_keys.erase(m_pending_keys.begin());
    return key;
}

void JsonUI::menu_show(ConstArrayView<DisplayLine> items,
                       CharCoord anchor, Face fg, Face bg,
                       MenuStyle style)
{
    rpc_call("menu_show", items, anchor, fg, bg, style);
}

void JsonUI::menu_select(int selected)
{
    rpc_call("menu_show", selected);
}

void JsonUI::menu_hide()
{
    rpc_call("menu_hide");
}

void JsonUI::info_show(StringView title, StringView content,
                       CharCoord anchor, Face face,
                       InfoStyle style)
{
    rpc_call("info_show", title, content, anchor, face, style);
}

void JsonUI::info_hide()
{
    rpc_call("info_hide");
}

void JsonUI::refresh(bool force)
{
    rpc_call("refresh", force);
}

void JsonUI::set_input_callback(InputCallback callback)
{
    m_input_callback = std::move(callback);
}

void JsonUI::set_ui_options(const Options& options)
{
    // rpc_call("set_ui_options", options);
}

CharCoord JsonUI::dimensions()
{
    return m_dimensions;
}

using JsonArray = Vector<Value>;
using JsonObject = IdMap<Value>;

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
        return Result{ Value{str_to_int({pos, end})}, digit_end };
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
        ++pos;
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
        ++pos;
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
            object.append({ std::move(name), std::move(element) });
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

void JsonUI::eval_json(const Value& json)
{
    const JsonObject& object = json.as<JsonObject>();
    auto json_it = object.find("jsonrpc");
    if (json_it == object.end() or json_it->value.as<String>() != "2.0")
        throw runtime_error("invalid json rpc request");

    auto method_it = object.find("method");
    if (method_it == object.end())
        throw runtime_error("invalid json rpc request (method missing)");
    StringView method = method_it->value.as<String>();

    auto params_it = object.find("params");
    if (params_it == object.end())
        throw runtime_error("invalid json rpc request (params missing)");
    const JsonArray& params = params_it->value.as<JsonArray>();

    if (method == "keys")
    {
        for (auto& key_val : params)
        {
            for (auto& key : parse_keys(key_val.as<String>()))
                m_pending_keys.push_back(key);
        }
    }
    else if (method == "resize")
    {
        if (params.size() != 2)
            throw runtime_error("resize expects 2 parameters");

        CharCoord dim{params[0].as<int>(), params[1].as<int>()};
        m_dimensions = dim;
        m_pending_keys.push_back(resize(dim));
    }
    else
        throw runtime_error("unknown method");
}

static bool stdin_ready()
{
    fd_set  rfds;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    return select(1, &rfds, nullptr, nullptr, &tv) == 1;
}

void JsonUI::parse_requests(EventMode mode)
{
    constexpr size_t bufsize = 1024;
    char buf[bufsize];
    while (stdin_ready())
    {
        ssize_t size = ::read(0, buf, bufsize);
        if (size == -1 or size == 0)
            break;

        m_requests += StringView{buf, buf + size};
    }

    if (not m_requests.empty())
    {
        Value json;
        const char* pos;
        std::tie(json, pos) = parse_json(m_requests.begin(), m_requests.end());
        if (json)
        {
            try
            {
                eval_json(json);
            }
            catch (runtime_error& error)
            {
                write_stderr(format("error while executing request '{}': '{}'",
                                    StringView{m_requests.begin(), pos}, error.what()));
            }
            m_requests = String{pos, m_requests.end()};
        }
    }

    while (not m_pending_keys.empty())
        m_input_callback(mode);
}

UnitTest test_json_parser{[]()
{
    StringView json = R"({ "jsonrpc": "2.0", "method": "keys", "params": [ "b", "l", "a", "h" ] })";
    auto value = std::get<0>(parse_json(json.begin(), json.end()));
    kak_assert(value);
}};

}
