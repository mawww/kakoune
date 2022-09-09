#include "client.hh"

#include "face_registry.hh"
#include "context.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "file.hh"
#include "remote.hh"
#include "option.hh"
#include "option_types.hh"
#include "client_manager.hh"
#include "command_manager.hh"
#include "event_manager.hh"
#include "user_interface.hh"
#include "window.hh"
#include "hash_map.hh"

#include <csignal>
#include <unistd.h>

#include <utility>

namespace Kakoune
{

Client::Client(std::unique_ptr<UserInterface>&& ui,
               std::unique_ptr<Window>&& window,
               SelectionList selections, int pid,
               EnvVarMap env_vars,
               String name,
               OnExitCallback on_exit)
    : m_ui{std::move(ui)}, m_window{std::move(window)},
      m_pid{pid},
      m_on_exit{std::move(on_exit)},
      m_env_vars(std::move(env_vars)),
      m_input_handler{std::move(selections), Context::Flags::None,
                      std::move(name)}
{
    m_window->set_client(this);

    context().set_client(*this);
    context().set_window(*m_window);

    m_window->set_dimensions(m_ui->dimensions());
    m_window->options().register_watcher(*this);

    m_ui->set_ui_options(m_window->options()["ui_options"].get<UserInterface::Options>());
    m_ui->set_on_key([this](Key key) {
        if (key == ctrl('c'))
        {
            auto prev_handler = set_signal_handler(SIGINT, SIG_IGN);
            killpg(getpgrp(), SIGINT);
            set_signal_handler(SIGINT, prev_handler);
        }
        else if (key.modifiers & Key::Modifiers::Resize)
        {
            m_window->set_dimensions(key.coord());
            force_redraw();
        }
        else
            m_pending_keys.push_back(key);
    });

    m_window->hooks().run_hook(Hook::WinDisplay, m_window->buffer().name(), context());

    force_redraw();
}

Client::~Client()
{
    m_window->options().unregister_watcher(*this);
    m_window->set_client(nullptr);
    // Do not move the selections here, as we need them to be valid
    // in order to correctly destroy the input handler
    ClientManager::instance().add_free_window(std::move(m_window),
                                              context().selections());
}

bool Client::is_ui_ok() const
{
    return m_ui->is_ok();
}

bool Client::process_pending_inputs()
{
    const bool debug_keys = (bool)(context().options()["debug"].get<DebugFlags>() & DebugFlags::Keys);
    m_window->run_resize_hook_ifn();
    // steal keys as we might receive new keys while handling them.
    Vector<Key, MemoryDomain::Client> keys = std::move(m_pending_keys);
    for (auto& key : keys)
    {
        try
        {
            if (debug_keys)
                write_to_debug_buffer(format("Client '{}' got key '{}'", context().name(), key));

            if (key == Key::FocusIn)
                context().hooks().run_hook(Hook::FocusIn, context().name(), context());
            else if (key == Key::FocusOut)
                context().hooks().run_hook(Hook::FocusOut, context().name(), context());
            else
                m_input_handler.handle_key(key);

            context().hooks().run_hook(Hook::RawKey, to_string(key), context());
        }
        catch (Kakoune::runtime_error& error)
        {
            write_to_debug_buffer(format("Error: {}", error.what()));
            context().print_status({error.what().str(), context().faces()["Error"] });
            context().hooks().run_hook(Hook::RuntimeError, error.what(), context());
        }
    }
    return not keys.empty();
}

void Client::print_status(DisplayLine status_line)
{
    m_status_line = std::move(status_line);
    m_ui_pending |= StatusLine;
}


DisplayCoord Client::dimensions() const
{
    return m_ui->dimensions();
}

String generate_context_info(const Context& context)
{
    String s = "";
    if (context.buffer().is_modified())
        s += "[+]";
    if (context.client().input_handler().is_recording())
        s += format("[recording ({})]", context.client().input_handler().recording_reg());
    if (context.hooks_disabled())
        s += "[no-hooks]";
    if (not(context.buffer().flags() & (Buffer::Flags::File | Buffer::Flags::Debug)))
        s += "[scratch]";
    if (context.buffer().flags() & Buffer::Flags::New)
        s += "[new file]";
    if (context.buffer().flags() & Buffer::Flags::Fifo)
        s += "[fifo]";
    if (context.buffer().flags() & Buffer::Flags::Debug)
        s += "[debug]";
    if (context.buffer().flags() & Buffer::Flags::ReadOnly)
        s += "[readonly]";
    return s;
}

DisplayLine Client::generate_mode_line() const
{
    DisplayLine modeline;
    try
    {
        const String& modelinefmt = context().options()["modelinefmt"].get<String>();
        HashMap<String, DisplayLine> atoms{{ "mode_info", context().client().input_handler().mode_line() },
                                           { "context_info", {generate_context_info(context()),
                                                              context().faces()["Information"]}}};
        auto expanded = expand(modelinefmt, context(), ShellContext{},
                               [](String s) { return escape(s, '{', '\\'); });
        modeline = parse_display_line(expanded, context().faces(), atoms);
    }
    catch (runtime_error& err)
    {
        write_to_debug_buffer(format("Error while parsing modelinefmt: {}", err.what()));
        modeline.push_back({ "modelinefmt error, see *debug* buffer", context().faces()["Error"] });
    }

    return modeline;
}

void Client::change_buffer(Buffer& buffer)
{
    if (m_buffer_reload_dialog_opened)
        close_buffer_reload_dialog();

    auto& client_manager = ClientManager::instance();
    WindowAndSelections ws = client_manager.get_free_window(buffer);

    m_window->options().unregister_watcher(*this);
    m_window->set_client(nullptr);
    client_manager.add_free_window(std::move(m_window),
                                   std::move(context().selections()));

    m_window = std::move(ws.window);
    m_window->set_client(this);
    m_window->options().register_watcher(*this);
    context().selections_write_only() = std::move(ws.selections);
    context().set_window(*m_window);

    m_window->set_dimensions(m_ui->dimensions());
    m_ui->set_ui_options(m_window->options()["ui_options"].get<UserInterface::Options>());

    m_window->hooks().run_hook(Hook::WinDisplay, buffer.name(), context());
    force_redraw();
}

static bool is_inline(InfoStyle style)
{
    return style == InfoStyle::Inline or
           style == InfoStyle::InlineAbove or
           style == InfoStyle::InlineBelow;
}

void Client::redraw_ifn()
{
    Window& window = context().window();
    if (window.needs_redraw(context()))
        m_ui_pending |= Draw;

    DisplayLine mode_line = generate_mode_line();
    if (mode_line.atoms() != m_mode_line.atoms())
    {
        m_ui_pending |= StatusLine;
        m_mode_line = std::move(mode_line);
    }

    if (m_ui_pending == 0)
        return;

    const auto& faces = context().faces();

    if (m_ui_pending & Draw)
        m_ui->draw(window.update_display_buffer(context()),
                   faces["Default"], faces["BufferPadding"]);

    const bool update_menu_anchor = (m_ui_pending & Draw) and not (m_ui_pending & MenuHide) and
                                    not m_menu.items.empty() and m_menu.style == MenuStyle::Inline;
    if ((m_ui_pending & MenuShow) or update_menu_anchor)
    {
        auto anchor = m_menu.style == MenuStyle::Inline ?
            window.display_position(m_menu.anchor) : DisplayCoord{};
        if (not (m_ui_pending & MenuShow) and m_menu.ui_anchor != anchor)
            m_ui_pending |= anchor ? (MenuShow | MenuSelect) : MenuHide;
        m_menu.ui_anchor = anchor;
    }

    if (m_ui_pending & MenuShow and m_menu.ui_anchor)
        m_ui->menu_show(m_menu.items, *m_menu.ui_anchor,
                        faces["MenuForeground"], faces["MenuBackground"],
                        m_menu.style);
    if (m_ui_pending & MenuSelect and m_menu.ui_anchor)
        m_ui->menu_select(m_menu.selected);
    if (m_ui_pending & MenuHide)
        m_ui->menu_hide();

    const bool update_info_anchor = (m_ui_pending & Draw) and not (m_ui_pending & InfoHide) and
                                    not m_info.content.empty() and is_inline(m_info.style);
    if ((m_ui_pending & InfoShow) or update_info_anchor)
    {
        auto anchor = is_inline(m_info.style) ?
             window.display_position(m_info.anchor) : DisplayCoord{};
        if (not (m_ui_pending & MenuShow) and m_info.ui_anchor != anchor)
            m_ui_pending |= anchor ? InfoShow : InfoHide;
        m_info.ui_anchor = anchor;
    }

    if (m_ui_pending & InfoShow and m_info.ui_anchor)
        m_ui->info_show(m_info.title, m_info.content, *m_info.ui_anchor,
                        faces["Information"], m_info.style);
    if (m_ui_pending & InfoHide)
        m_ui->info_hide();

    if (m_ui_pending & StatusLine)
        m_ui->draw_status(m_status_line, m_mode_line, faces["StatusLine"]);

    auto cursor = m_input_handler.get_cursor_info();
    m_ui->set_cursor(cursor.first, cursor.second);

    m_ui->refresh(m_ui_pending & Refresh);
    m_ui_pending = 0;
}

void Client::force_redraw()
{
    m_ui_pending |= Refresh | Draw | StatusLine |
        (m_menu.items.empty() ? MenuHide : MenuShow | MenuSelect) |
        (m_info.content.empty() ? InfoHide : InfoShow);
}

void Client::reload_buffer()
{
    Buffer& buffer = context().buffer();
    try
    {
        reload_file_buffer(buffer);
        context().print_status({ format("'{}' reloaded", buffer.display_name()),
                                 context().faces()["Information"] });

        m_window->hooks().run_hook(Hook::BufReload, buffer.name(), context());
    }
    catch (runtime_error& error)
    {
        context().print_status({ format("error while reloading buffer: '{}'", error.what()),
                                 context().faces()["Error"] });
        buffer.set_fs_status(get_fs_status(buffer.name()));
    }
}

void Client::on_buffer_reload_key(Key key)
{
    auto& buffer = context().buffer();

    auto set_autoreload = [this](Autoreload autoreload) {
        auto* option = &context().options()["autoreload"];
        // Do not touch global autoreload, set it at least at buffer level
        if (&option->manager() == &GlobalScope::instance().options())
            option = &context().buffer().options().get_local_option("autoreload");
        option->set(autoreload);
    };

    if (key == 'y' or key == 'Y' or key == Key::Return)
    {
        reload_buffer();
        if (key == 'Y')
            set_autoreload(Autoreload::Yes);
    }
    else if (key == 'n' or key == 'N' or key == Key::Escape)
    {
        // reread timestamp in case the file was modified again
        buffer.set_fs_status(get_fs_status(buffer.name()));
        print_status({ format("'{}' kept", buffer.display_name()),
                       context().faces()["Information"] });
        if (key == 'N')
            set_autoreload(Autoreload::No);
    }
    else
    {
        print_status({ format("'{}' is not a valid choice", key),
                       context().faces()["Error"] });
        m_input_handler.on_next_key("buffer-reload", KeymapMode::None, [this](Key key, Context&){ on_buffer_reload_key(key); });
        return;
    }

    for (auto& client : ClientManager::instance())
    {
        if (&client->context().buffer() == &buffer and
            client->m_buffer_reload_dialog_opened)
            client->close_buffer_reload_dialog();
    }
}

void Client::close_buffer_reload_dialog()
{
    kak_assert(m_buffer_reload_dialog_opened);
    // Reset first as this might check for reloading.
    m_input_handler.reset_normal_mode();
    m_buffer_reload_dialog_opened = false;
    info_hide(true);
}

void Client::check_if_buffer_needs_reloading()
{
    if (m_buffer_reload_dialog_opened)
        return;

    Buffer& buffer = context().buffer();
    auto reload = context().options()["autoreload"].get<Autoreload>();
    if (not (buffer.flags() & Buffer::Flags::File) or reload == Autoreload::No)
        return;

    const String& filename = buffer.name();
    const timespec ts = get_fs_timestamp(filename);
    const auto status = buffer.fs_status();

    if (ts == InvalidTime or ts == status.timestamp)
        return;

    if (MappedFile fd{filename};
        fd.st.st_size == status.file_size and hash_data(fd.data, fd.st.st_size) == status.hash)
        return;

    if (reload == Autoreload::Ask)
    {
        StringView bufname = buffer.display_name();
        info_show(format("reload '{}' ?", bufname),
                  format("'{}' was modified externally\n"
                         " y, <ret>: reload | n, <esc>: keep\n"
                         " Y: always reload | N: always keep\n",
                         bufname), {}, InfoStyle::Modal);

        m_buffer_reload_dialog_opened = true;
        m_input_handler.on_next_key("buffer-reload", KeymapMode::None, [this](Key key, Context&){ on_buffer_reload_key(key); });
    }
    else
        reload_buffer();
}

StringView Client::get_env_var(StringView name) const
{
    auto it = m_env_vars.find(name);
    if (it == m_env_vars.end())
        return {};
    return it->value;
}

void Client::on_option_changed(const Option& option)
{
    if (option.name() == "ui_options")
    {
        m_ui->set_ui_options(option.get<UserInterface::Options>());
        m_ui_pending |= Draw;
    }
}

void Client::menu_show(Vector<DisplayLine> choices, BufferCoord anchor, MenuStyle style)
{
    m_menu = Menu{ std::move(choices), anchor, {}, style, -1 };
    m_ui_pending |= MenuShow;
    m_ui_pending &= ~MenuHide;
}

void Client::menu_select(int selected)
{
    m_menu.selected = selected;
    m_ui_pending |= MenuSelect;
    m_ui_pending &= ~MenuHide;
}

void Client::menu_hide()
{
    m_menu = Menu{};
    m_ui_pending |= MenuHide;
    m_ui_pending &= ~(MenuShow | MenuSelect);
}

void Client::info_show(DisplayLine title, DisplayLineList content, BufferCoord anchor, InfoStyle style)
{
    if (m_info.style == InfoStyle::Modal) // We already have a modal info opened, do not touch it.
        return;

    m_info = Info{ std::move(title), std::move(content), anchor, {}, style };
    m_ui_pending |= InfoShow;
    m_ui_pending &= ~InfoHide;
}

void Client::info_show(StringView title, StringView content, BufferCoord anchor, InfoStyle style)
{
    if (not content.empty() and content.back() == '\n')
        content = content.substr(0, content.length() - 1);
    info_show(title.empty() ? DisplayLine{} : DisplayLine{title.str(), Face{}},
              content | split<StringView>('\n')
                      | transform([](StringView s) { return DisplayLine{replace(s, '\t', ' '), Face{}}; })
                      | gather<DisplayLineList>(),
              anchor, style);
}

void Client::info_hide(bool even_modal)
{
    if (not even_modal and m_info.style == InfoStyle::Modal)
        return;

    m_info = Info{};
    m_ui_pending |= InfoHide;
    m_ui_pending &= ~InfoShow;
}

}
