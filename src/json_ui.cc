#include "json_ui.hh"

#include "display_buffer.hh"
#include "event_manager.hh"
#include "exception.hh"
#include "file.hh"
#include "json.hh"
#include "keys.hh"
#include "ranges.hh"
#include "string_utils.hh"

#include <cstdio>
#include <utility>

#include <unistd.h>

namespace Kakoune
{

struct invalid_rpc_request : runtime_error {
    invalid_rpc_request(const String& message)
        : runtime_error(format("invalid json rpc request ({})", message)) {}
};

String to_json(Color color)
{
    if (color.color == Kakoune::Color::RGB)
    {
        char buffer[10];
        sprintf(buffer, R"("#%02x%02x%02x")", color.r, color.g, color.b);
        return buffer;
    }
    return to_json(to_string(color));
}

String to_json(Attribute attributes)
{
    struct Attr { Attribute attr; StringView name; }
    attrs[] {
        { Attribute::Underline, "underline" },
        { Attribute::Reverse, "reverse" },
        { Attribute::Blink, "blink" },
        { Attribute::Bold, "bold" },
        { Attribute::Dim, "dim" },
        { Attribute::Italic, "italic" },
        { Attribute::FinalFg, "final_fg" },
        { Attribute::FinalBg, "final_bg" },
        { Attribute::FinalAttr, "final_attr" },
        { Attribute::Strikethrough, "strikethrough" },
    };

    return "[" + join(attrs |
                      filter([=](const Attr& a) { return attributes & a.attr; }) |
                      transform([](const Attr& a) { return to_json(a.name); }),
                      ',', false) + "]";
}

String to_json(Face face)
{
    return format(R"(\{ "fg": {}, "bg": {}, "underline": {}, "attributes": {} })",
                  to_json(face.fg), to_json(face.bg), to_json(face.underline), to_json(face.attributes));
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
        case MenuStyle::Search: return R"("search")";
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
    : m_stdin_watcher{0, FdEvents::Read, EventMode::Urgent,
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

void JsonUI::info_show(const DisplayLine& title, const DisplayLineList& content,
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
    rpc_call("set_ui_options", options);
}

DisplayCoord JsonUI::dimensions()
{
    return m_dimensions;
}

void JsonUI::set_on_key(OnKeyCallback callback)
{
    m_on_key = std::move(callback);
}

void JsonUI::eval_json(const Value& json)
{
    if (not json.is_a<JsonObject>())
        throw invalid_rpc_request("request is not an object");

    const JsonObject& object = json.as<JsonObject>();
    auto json_it = object.find("jsonrpc"_sv);
    if (json_it == object.end() or
        not json_it->value.is_a<String>() or
        json_it->value.as<String>() != "2.0")
        throw invalid_rpc_request("only protocol '2.0' is supported");
    else if (not json_it->value.is_a<String>())
        throw invalid_rpc_request("'jsonrpc' is not a string");

    auto method_it = object.find("method"_sv);
    if (method_it == object.end())
        throw invalid_rpc_request("method missing");
    else if (not method_it->value.is_a<String>())
        throw invalid_rpc_request("'method' is not a string");
    StringView method = method_it->value.as<String>();

    auto params_it = object.find("params"_sv);
    if (params_it == object.end())
        throw invalid_rpc_request("params missing");
    else if (not params_it->value.is_a<JsonArray>())
        throw invalid_rpc_request("'params' is not an array");
    const JsonArray& params = params_it->value.as<JsonArray>();

    if (method == "keys")
    {
        for (auto& key_val : params)
        {
            if (not key_val.is_a<String>())
                throw invalid_rpc_request("'keys' is not an array of strings");

            for (auto& key : parse_keys(key_val.as<String>()))
                m_on_key(key);
        }
    }
    else if (method == "mouse_move")
    {
        if (params.size() != 2)
            throw invalid_rpc_request("mouse coordinates not specified");

        if (not params[0].is_a<int>() or not params[1].is_a<int>())
            throw invalid_rpc_request("mouse coordinates are not integers");

        m_on_key({Key::Modifiers::MousePos, encode_coord({params[0].as<int>(), params[1].as<int>()})});
    }
    else if (method == "mouse_press" or method == "mouse_release")
    {
        if (params.size() != 3)
            throw invalid_rpc_request("mouse button/coordinates not specified");

        if (not params[0].is_a<String>())
            throw invalid_rpc_request("mouse button is not a string");
        if (not params[1].is_a<int>() or not params[2].is_a<int>())
            throw invalid_rpc_request("mouse coordinates are not integers");

        auto event = method == "mouse_press" ? Key::Modifiers::MousePress : Key::Modifiers::MouseRelease;
        auto button = str_to_button(params[0].as<String>());

        m_on_key({event | Key::to_modifier(button), encode_coord({params[1].as<int>(), params[2].as<int>()})});
    }
    else if (method == "scroll")
    {
        if (params.size() != 1)
            throw invalid_rpc_request("scroll needs an amount");
        else if (not params[0].is_a<int>())
            throw invalid_rpc_request("scroll amount is not an integer");
        m_on_key({Key::Modifiers::Scroll, (Codepoint)params[0].as<int>()});

    }
    else if (method == "menu_select")
    {
        if (params.size() != 1)
            throw invalid_rpc_request("menu_select needs the item index");
        else if (not params[0].is_a<int>())
            throw invalid_rpc_request("menu index is not an integer");

        m_on_key({Key::Modifiers::MenuSelect, (Codepoint)params[0].as<int>()});
    }
    else if (method == "resize")
    {
        if (params.size() != 2)
            throw runtime_error("resize expects 2 parameters");
        else if (not params[0].is_a<int>() or
                 not params[1].is_a<int>())
            throw invalid_rpc_request("width and height are not integers");

        DisplayCoord dim{params[0].as<int>(), params[1].as<int>()};
        m_dimensions = dim;
        m_on_key(resize(dim));
    }
    else
        throw invalid_rpc_request(format("unknown method: {}", method));
}

void JsonUI::parse_requests(EventMode mode)
{
    constexpr size_t bufsize = 1024;
    char buf[bufsize];
    while (fd_readable(0))
    {
        ssize_t size = ::read(0, buf, bufsize);
        if (size == -1 or size == 0)
        {
            m_stdin_watcher.close_fd();
            break;
        }

        m_requests += StringView{buf, buf + size};
    }

    if (not m_on_key)
        return;

    while (not m_requests.empty())
    {
        const char* pos = nullptr;
        try
        {
            auto [json, new_pos] = parse_json(m_requests);
            pos = new_pos;
            if (json)
                eval_json(json);
        }
        catch (runtime_error& error)
        {
            write(2, format("error while handling requests '{}': '{}'\n",
                            m_requests, error.what()));
            // try to salvage request by dropping its first line
            pos = std::min(m_requests.end(), find(m_requests, '\n')+1);
        }
        if (not pos)
            break; // unterminated request ?

        m_requests = String{pos, m_requests.end()};
    }
}

}
