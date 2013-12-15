#include "context.hh"

#include "client.hh"
#include "user_interface.hh"
#include "window.hh"

namespace Kakoune
{

Context::Context() = default;

Context::Context(InputHandler& input_handler, Editor& editor, String name)
    : m_input_handler(&input_handler), m_editor(&editor),
      m_name(std::move(name)) {}

Context::~Context() = default;

Buffer& Context::buffer() const
{
    if (not has_buffer())
        throw runtime_error("no buffer in context");
    return m_editor->buffer();
}

Editor& Context::editor() const
{
    if (not has_editor())
        throw runtime_error("no editor in context");
    return *m_editor.get();
}

Window& Context::window() const
{
    if (not has_window())
        throw runtime_error("no window in context");
    return *dynamic_cast<Window*>(m_editor.get());
}

bool Context::has_window() const
{
    return (bool)m_editor and dynamic_cast<Window*>(m_editor.get());
}

InputHandler& Context::input_handler() const
{
    if (not has_input_handler())
        throw runtime_error("no input handler in context");
    return *m_input_handler;
}

Client& Context::client() const
{
    if (not has_client())
        throw runtime_error("no client in context");
    return *m_client;
}

UserInterface& Context::ui() const
{
    if (not has_ui())
        throw runtime_error("no user interface in context");
    return client().ui();
}

OptionManager& Context::options() const
{
    if (has_window())
        return window().options();
    if (has_buffer())
        return buffer().options();
    return GlobalOptions::instance();
}

HookManager& Context::hooks() const
{
    if (has_window())
        return window().hooks();
    if (has_buffer())
        return buffer().hooks();
    return GlobalHooks::instance();
}

KeymapManager& Context::keymaps() const
{
    if (has_window())
        return window().keymaps();
    if (has_buffer())
        return buffer().keymaps();
    return GlobalKeymaps::instance();
}

void Context::set_client(Client& client)
{
    kak_assert(not has_client());
    m_client.reset(&client);
}

void Context::print_status(DisplayLine status) const
{
    if (has_client())
        client().print_status(std::move(status));
}

void Context::push_jump()
{
    const SelectionList& jump = selections();
    if (m_current_jump != m_jump_list.end())
    {
        auto begin = m_current_jump;
        if (&editor().buffer() != &begin->buffer() or
            (const SelectionList&)(*begin) != jump)
            ++begin;
        m_jump_list.erase(begin, m_jump_list.end());
    }
    m_jump_list.erase(std::remove(begin(m_jump_list), end(m_jump_list), jump),
                      end(m_jump_list));
    m_jump_list.push_back({editor().buffer(), jump});
    m_current_jump = m_jump_list.end();
}

const DynamicSelectionList& Context::jump_forward()
{
    if (m_current_jump != m_jump_list.end() and
        m_current_jump + 1 != m_jump_list.end())
        return *++m_current_jump;
    throw runtime_error("no next jump");
}

const DynamicSelectionList& Context::jump_backward()
{
    if (m_current_jump != m_jump_list.end() and
        *m_current_jump != selections())
    {
        push_jump();
        return *--m_current_jump;
    }
    if (m_current_jump != m_jump_list.begin())
    {
        if (m_current_jump == m_jump_list.end())
        {
            push_jump();
            --m_current_jump;
        }
        return *--m_current_jump;
    }
    throw runtime_error("no previous jump");
}

void Context::forget_jumps_to_buffer(Buffer& buffer)
{
    for (auto it = m_jump_list.begin(); it != m_jump_list.end();)
    {
        if (&it->buffer() == &buffer)
        {
            if (it < m_current_jump)
                --m_current_jump;
            else if (it == m_current_jump)
                m_current_jump = m_jump_list.end()-1;

            it = m_jump_list.erase(it);
        }
        else
            ++it;
    }
}

void Context::change_editor(Editor& editor)
{
    m_editor.reset(&editor);
    if (has_window())
    {
        if (has_ui())
            window().set_dimensions(ui().dimensions());
        window().hooks().run_hook("WinDisplay", buffer().name(), *this);
    }
    if (has_input_handler())
        input_handler().reset_normal_mode();
}

SelectionList& Context::selections()
{
    return editor().selections();
}

const SelectionList& Context::selections() const
{
    return editor().selections();
}

void Context::begin_edition()
{
    ++m_edition_level;
}

void Context::end_edition()
{
    kak_assert(m_edition_level > 0);
    if (m_edition_level == 1)
        buffer().commit_undo_group();

    --m_edition_level;
}

}
