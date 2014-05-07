#include "client.hh"

#include "color_registry.hh"
#include "context.hh"
#include "buffer_manager.hh"
#include "user_interface.hh"
#include "file.hh"
#include "remote.hh"
#include "client_manager.hh"
#include "window.hh"

namespace Kakoune
{

Client::Client(std::unique_ptr<UserInterface>&& ui,
               std::unique_ptr<Window>&& window,
               SelectionList selections,
               EnvVarMap env_vars,
               String name)
    : m_ui{std::move(ui)}, m_window{std::move(window)},
      m_input_handler{m_window->buffer(), std::move(selections),
                      std::move(name)},
      m_env_vars(env_vars)
{
    context().set_client(*this);
    context().set_window(*m_window);
}

Client::~Client()
{
}

void Client::handle_available_input()
{
    while (m_ui->is_key_available())
    {
        m_input_handler.handle_key(m_ui->get_key());
        m_input_handler.clear_mode_trash();
    }
    context().window().forget_timestamp();
}

void Client::print_status(DisplayLine status_line)
{
    m_status_line = std::move(status_line);
    context().window().forget_timestamp();
}

DisplayLine Client::generate_mode_line() const
{
    auto pos = context().selections().main().cursor();
    auto col = context().buffer()[pos.line].char_count_to(pos.column);

    std::ostringstream oss;
    oss << context().buffer().display_name()
        << " " << (int)pos.line+1 << ":" << (int)col+1;
    if (context().buffer().is_modified())
        oss << " [+]";
    if (m_input_handler.is_recording())
       oss << " [recording (" << m_input_handler.recording_reg() << ")]";
    if (context().buffer().flags() & Buffer::Flags::New)
        oss << " [new file]";
    oss << " [" << m_input_handler.mode_string() << "]" << " - "
        << context().name() << "@[" << Server::instance().session() << "]";
    return { oss.str(), get_color("StatusLine") };
}

void Client::change_buffer(Buffer& buffer)
{
    auto& client_manager = ClientManager::instance();
    client_manager.add_free_window(std::move(m_window),
                                   std::move(context().selections()));
    WindowAndSelections ws = client_manager.get_free_window(buffer);
    m_window = std::move(ws.window);
    context().m_selections = std::move(ws.selections);
    context().set_window(*m_window);
    m_window->set_dimensions(ui().dimensions());
    m_window->hooks().run_hook("WinDisplay", buffer.name(), context());
}

void Client::redraw_ifn()
{
    if (context().window().timestamp() != context().buffer().timestamp())
    {
        CharCoord dimensions = context().ui().dimensions();
        if (dimensions == CharCoord{0,0})
            return;
        context().window().set_dimensions(dimensions);
        context().window().update_display_buffer(context());

        context().ui().draw(context().window().display_buffer(),
                            m_status_line, generate_mode_line());
    }
    context().ui().refresh();
}

static void reload_buffer(Context& context, const String& filename)
{
    CharCoord view_pos = context.window().position();
    ByteCoord cursor_pos = context.selections().main().cursor();
    Buffer* buf = create_buffer_from_file(filename);
    if (not buf)
        return;
    context.change_buffer(*buf);
    context.selections() = SelectionList{buf->clamp(cursor_pos)};
    context.window().set_position(view_pos);
    context.print_status({ "'" + buf->display_name() + "' reloaded",
                           get_color("Information") });
}

void Client::check_buffer_fs_timestamp()
{
    Buffer& buffer = context().buffer();
    auto reload = context().options()["autoreload"].get<YesNoAsk>();
    if (not (buffer.flags() & Buffer::Flags::File) or reload == No)
        return;

    const String& filename = buffer.name();
    time_t ts = get_fs_timestamp(filename);
    if (ts == buffer.fs_timestamp())
        return;
    if (reload == Ask)
    {
        CharCoord pos = context().window().dimensions();
        pos.column -= 1;
        m_ui->info_show(
            "reload '" + buffer.display_name() + "' ?",
            "'" + buffer.display_name() + "' was modified externally\n"
            "press r or y to reload, k or n to keep",
            pos, get_color("Information"), MenuStyle::Prompt);

        m_input_handler.on_next_key([this, filename, ts](Key key, Context& context) {
            Buffer* buf = BufferManager::instance().get_buffer_ifp(filename);
            m_ui->info_hide();
            // buffer got deleted while waiting for the key, do nothing
            if (not buf)
                return;
            if (key == 'r' or key == 'y')
                reload_buffer(context, filename);
            else if (key == 'k' or key == 'n')
            {
                buf->set_fs_timestamp(ts);
                print_status({ "'" + buf->display_name() + "' kept",
                               get_color("Information") });
            }
            else
            {
                print_status({ "'" + key_to_str(key) + "' is not a valid choice",
                               get_color("Error") });
                check_buffer_fs_timestamp();
            }
        });
    }
    else
        reload_buffer(context(), filename);
}

const String& Client::get_env_var(const String& name) const
{
    auto it = m_env_vars.find(name);
    static String empty{};
    if (it == m_env_vars.end())
        return empty;
    return it->second;
}

}
