#include "context.hh"

#include "alias_registry.hh"
#include "client.hh"
#include "face_registry.hh"
#include "buffer_manager.hh"
#include "register_manager.hh"
#include "window.hh"

namespace Kakoune
{

Context::~Context() = default;

Context::Context(InputHandler& input_handler, SelectionList selections,
                 Flags flags, String name)
    : m_flags(flags),
      m_input_handler{&input_handler},
      m_selections{std::move(selections)},
      m_name(std::move(name))
{}

Context::Context(EmptyContextFlag) {}

Buffer& Context::buffer() const
{
    if (not has_buffer())
        throw runtime_error("no buffer in context");
    return const_cast<Buffer&>((*m_selections).buffer());
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

Scope& Context::scope() const
{
    if (has_window())
        return window();
    if (has_buffer())
        return buffer();
    return GlobalScope::instance();
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

void JumpList::push(SelectionList jump, Optional<size_t> index)
{
    if (index)
    {
        m_current = *index;
        kak_assert(m_current <= m_jumps.size());
    }

    if (m_current != m_jumps.size())
        m_jumps.erase(m_jumps.begin()+m_current+1, m_jumps.end());
    m_jumps.erase(std::remove(begin(m_jumps), end(m_jumps), jump),
                      end(m_jumps));
    m_jumps.push_back(jump);
    m_current = m_jumps.size();
}

const SelectionList& JumpList::forward(Context& context, int count)
{
    if (m_current != m_jumps.size() and
        m_current + count < m_jumps.size())
    {
        m_current += count;
        SelectionList& res = m_jumps[m_current];
        res.update();
        context.print_status({ format("jumped to #{} ({})",
                               m_current, m_jumps.size() - 1),
                               context.faces()["Information"] });
        return res;
    }
    throw runtime_error("no next jump");
}

const SelectionList& JumpList::backward(Context& context, int count)
{
    if ((int)m_current - count < 0)
        throw runtime_error("no previous jump");

    const SelectionList& current = context.selections();
    if (m_current != m_jumps.size() and
        m_jumps[m_current] != current)
    {
        push(current);
        m_current -= count;
        SelectionList& res = m_jumps[m_current];
        res.update();
        context.print_status({ format("jumped to #{} ({})",
                               m_current, m_jumps.size() - 1),
                               context.faces()["Information"] });
        return res;
    }
    if (m_current != 0)
    {
        if (m_current == m_jumps.size())
        {
            push(current);
            if (--m_current == 0)
                throw runtime_error("no previous jump");
        }
        m_current -= count;
        SelectionList& res = m_jumps[m_current];
        res.update();
        context.print_status({ format("jumped to #{} ({})",
                               m_current, m_jumps.size() - 1),
                               context.faces()["Information"] });
        return res;
    }
    throw runtime_error("no previous jump");
}

void JumpList::forget_buffer(Buffer& buffer)
{
    for (size_t i = 0; i < m_jumps.size();)
    {
        if (&m_jumps[i].buffer() == &buffer)
        {
            if (i < m_current)
                --m_current;
            else if (i == m_current)
                m_current = m_jumps.size()-1;

            m_jumps.erase(m_jumps.begin() + i);
        }
        else
            ++i;
    }
}

void Context::change_buffer(Buffer& buffer)
{
    if (has_buffer() and &buffer == &this->buffer())
        return;

    if (has_buffer() and m_edition_level > 0)
       this->buffer().commit_undo_group();

    if (has_client())
    {
        client().info_hide();
        client().menu_hide();
        client().change_buffer(buffer);
    }
    else
    {
        m_window.reset();
        m_selections = SelectionList{buffer, Selection{}};
    }

    if (has_input_handler())
        input_handler().reset_normal_mode();
}

void Context::forget_buffer(Buffer& buffer)
{
    m_jump_list.forget_buffer(buffer);

    if (&this->buffer() != &buffer)
        return;

    if (is_editing() && has_input_handler())
        input_handler().reset_normal_mode();

    auto last_buffer = this->last_buffer();
    change_buffer(last_buffer ? *last_buffer : BufferManager::instance().get_first_buffer());
}

Buffer* Context::last_buffer() const
{
    const auto jump_list = m_jump_list.get_as_list();
    if (jump_list.empty())
        return nullptr;

    auto predicate = [this](const auto& sels) {
        return &sels.buffer() != &this->buffer();
    };

    auto next_buffer = find_if(jump_list.subrange(m_jump_list.current_index()-1),
                               predicate);
    if (next_buffer != jump_list.end())
        return &next_buffer->buffer();

    auto previous_buffer = find_if(jump_list.subrange(0, m_jump_list.current_index()) | reverse(),
                                   predicate);

    return previous_buffer != jump_list.rend() ? &previous_buffer->buffer() : nullptr;
}

SelectionList& Context::selections()
{
    if (not m_selections)
        throw runtime_error("no selections in context");
    (*m_selections).update();
    return *m_selections;
}

SelectionList& Context::selections_write_only()
{
    if (not m_selections)
        throw runtime_error("no selections in context");
    return *m_selections;
}

const SelectionList& Context::selections() const
{
    return const_cast<Context&>(*this).selections();
}

Vector<String> Context::selections_content() const
{
    auto& buf = buffer();
    Vector<String> contents;
    for (auto& sel : selections())
        contents.push_back(buf.string(sel.min(), buf.char_next(sel.max())));
    return contents;
}

void Context::begin_edition()
{
    if (m_edition_level >= 0)
    {
        if (m_edition_level == 0)
            m_edition_timestamp = buffer().timestamp();
        ++m_edition_level;
    }
}

void Context::end_edition()
{
    if (m_edition_level < 0)
        return;

    kak_assert(m_edition_level != 0);
    if (m_edition_level == 1 and
        buffer().timestamp() != m_edition_timestamp)
        buffer().commit_undo_group();

    --m_edition_level;
}

StringView Context::main_sel_register_value(StringView reg) const
{
    size_t index = m_selections ? (*m_selections).main_index() : 0;
    return RegisterManager::instance()[reg].get_main(*this, index);
}

}
