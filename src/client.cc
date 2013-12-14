#include "client.hh"

#include "color_registry.hh"
#include "context.hh"
#include "editor.hh"
#include "buffer_manager.hh"
#include "user_interface.hh"
#include "file.hh"
#include "remote.hh"
#include "client_manager.hh"
#include "window.hh"

namespace Kakoune
{

Client::Client(std::unique_ptr<UserInterface>&& ui, Editor& editor, String name)
    : m_input_handler(editor, std::move(name)), m_ui(std::move(ui))
{
    context().set_client(*this);
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
    auto pos = context().editor().selections().main().last();
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

void Client::redraw_ifn()
{
    if (context().window().timestamp() != context().buffer().timestamp())
    {
        DisplayCoord dimensions = context().ui().dimensions();
        if (dimensions == DisplayCoord{0,0})
            return;
        context().window().set_dimensions(dimensions);
        context().window().update_display_buffer();;

        context().ui().draw(context().window().display_buffer(),
                            m_status_line, generate_mode_line());
    }
}

static void reload_buffer(Context& context, const String& filename)
{
    DisplayCoord view_pos = context.window().position();
    BufferCoord cursor_pos = context.editor().selections().main().last();
    Buffer* buf = create_buffer_from_file(filename);
    if (not buf)
        return;
    Window& win = ClientManager::instance().get_unused_window_for_buffer(*buf);
    win.select(cursor_pos);
    win.set_position(view_pos);
    context.change_editor(win);
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
        print_status({"'" + buffer.display_name() + "' was modified externally, press r or y to reload, k or n to keep", get_color("Prompt")});
        m_input_handler.on_next_key([this, ts, filename](Key key, Context& context) {
            Buffer* buf = BufferManager::instance().get_buffer_ifp(filename);
            // buffer got deleted while waiting for the key, do nothing
            if (not buf)
            {
                print_status({});
                return;
            }
            if (key == 'r' or key == 'y')
                reload_buffer(context, filename);
            if (key == 'k' or key == 'n')
            {
                buf->set_fs_timestamp(ts);
                print_status({"'" + buf->display_name() + "' kept", get_color("Information") });
            }
            else
                check_buffer_fs_timestamp();
        });
    }
    else
        reload_buffer(context(), filename);
}

}
