#include "context.hh"

#include "client.hh"
#include "user_interface.hh"
#include "window.hh"

namespace Kakoune
{

Context::Context() = default;
Context::~Context() = default;

Context::Context(InputHandler& input_handler, Buffer& buffer, SelectionList selections, String name)
    : m_input_handler{&input_handler}, m_selections{{buffer, std::move(selections)}},
      m_name(std::move(name)) {}

Buffer& Context::buffer() const
{
    if (not has_buffer())
        throw runtime_error("no buffer in context");
    return (*m_selections).registry();
}

Window& Context::window() const
{
    if (not has_window())
        throw runtime_error("no window in context");
    return *m_window;
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

void Context::set_window(Window& window)
{
    kak_assert(&window.buffer() == &buffer());
    m_window.reset(&window);
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
        if (&buffer() != &begin->buffer() or
            (const SelectionList&)(*begin) != jump)
            ++begin;
        m_jump_list.erase(begin, m_jump_list.end());
    }
    m_jump_list.erase(std::remove(begin(m_jump_list), end(m_jump_list), jump),
                      end(m_jump_list));
    m_jump_list.push_back({buffer(), jump});
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

void Context::change_buffer(Buffer& buffer)
{
    m_window.reset();
    if (has_client())
        client().change_buffer(buffer);
    else
        m_selections = DynamicSelectionList{ buffer };
    if (has_input_handler())
        input_handler().reset_normal_mode();
}

SelectionList& Context::selections()
{
    if (not m_selections)
        throw runtime_error("no selections in context");
    return *m_selections;
}

const SelectionList& Context::selections() const
{
    if (not m_selections)
        throw runtime_error("no selections in context");
    return *m_selections;
}

std::vector<String> Context::selections_content() const
{
    std::vector<String> contents;
    for (auto& sel : selections())
        contents.push_back(buffer().string(sel.min(), buffer().char_next(sel.max())));
    return contents;
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
