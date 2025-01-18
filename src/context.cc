#include "context.hh"

#include "client.hh"
#include "scope.hh"
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
      m_selection_history{*this, std::move(selections)},
      m_name(std::move(name))
{}

Context::Context(EmptyContextFlag) : m_selection_history{*this} {}

Buffer& Context::buffer() const
{
    if (not has_buffer())
        throw runtime_error("no buffer in context");
    return const_cast<Buffer&>(selections(false).buffer());
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

Scope& Context::scope(bool allow_local) const
{
    if (allow_local and not m_local_scopes.empty())
        return *m_local_scopes.back();
    if (has_window())
        return window();
    if (has_buffer())
        return buffer();
    return GlobalScope::instance();
}

OptionManager& Context::options() const { return scope().options(); }
HookManager&   Context::hooks()   const { return scope().hooks(); }
KeymapManager& Context::keymaps() const { return scope().keymaps(); }
AliasRegistry& Context::aliases() const { return scope().aliases(); }
FaceRegistry&  Context::faces(bool allow_local) const { return scope(allow_local).faces(); }

void Context::set_client(Client& client)
{
    kak_assert(not has_client());
    m_client.reset(&client);
}

void Context::set_window(Window& window)
{
    kak_assert(&window.buffer() == &buffer());
    m_window.reset(&window);
    if (not m_local_scopes.empty())
        m_local_scopes.front()->reparent(window);
}

void Context::print_status(DisplayLine status) const
{
    if (has_client())
        client().print_status(std::move(status));
}

void Context::push_jump(bool force)
{
    if (force or not (m_flags & Flags::Draft))
        m_jump_list.push(selections());
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

Context::SelectionHistory::SelectionHistory(Context& context) : m_context(context) {}

Context::SelectionHistory::SelectionHistory(Context& context, SelectionList selections)
    : m_context(context),
      m_history{HistoryNode{std::move(selections), HistoryId::Invalid}},
      m_history_id(HistoryId::First) {}

void Context::SelectionHistory::initialize(SelectionList selections)
{
    kak_assert(empty());
    m_history = {HistoryNode{std::move(selections), HistoryId::Invalid}};
    m_history_id = HistoryId::First;
}

SelectionList& Context::SelectionHistory::selections(bool update)
{
    if (empty())
        throw runtime_error("no selections in context");
    auto& sels = m_staging ? m_staging->selections : current_history_node().selections;
    if (update)
        sels.update();
    return sels;
}

void Context::SelectionHistory::begin_edition()
{
    if (not in_edition())
        m_staging = HistoryNode{selections(), m_history_id};
    m_in_edition.set();
}

void Context::SelectionHistory::end_edition()
{
    kak_assert(in_edition());
    m_in_edition.unset();
    if (in_edition())
        return;

    if (m_history_id != HistoryId::Invalid and current_history_node().selections == m_staging->selections)
    {
        // No change, except maybe the index of the main selection.
        // Update timestamp to potentially improve interaction with content undo.
        auto& node = current_history_node();
        node.selections.force_timestamp(m_staging->selections.timestamp());
        node.selections.set_main_index(m_staging->selections.main_index());
    }
    else
    {
        m_history_id = next_history_id();
        m_history.push_back(std::move(*m_staging));
    }
    m_staging.reset();
}

template<Direction direction>
void Context::SelectionHistory::undo()
{
    static constexpr bool backward = direction == Backward;
    if (in_edition())
        throw runtime_error("selection undo is only supported at top-level");
    kak_assert(not empty());
    SelectionList old_selections = selections();
    HistoryId next;
    do
    {
        if constexpr (backward)
            next = current_history_node().parent;
        else
            next = current_history_node().redo_child;
        if (next == HistoryId::Invalid)
            throw runtime_error(backward ? "no selection change to undo" : "no selection change to redo");
        auto select_next = [&, next] {
            HistoryId previous_id = m_history_id;
            m_history_id = next;
            if constexpr (backward)
                current_history_node().redo_child = previous_id;
        };
        Buffer& destination_buffer = history_node(next).selections.buffer();
        if (&destination_buffer == &m_context.buffer())
            select_next();
        else
            m_context.change_buffer(destination_buffer, { std::move(select_next) });
    }
    while (selections() == old_selections);
}

void Context::SelectionHistory::forget_buffer(Buffer& buffer)
{
    Vector<HistoryId, MemoryDomain::Selections> new_ids;
    size_t bias = 0;
    for (size_t i = 0; i < m_history.size(); ++i)
    {
        auto& node = history_node((HistoryId)i);
        HistoryId id;
        if (&node.selections.buffer() == &buffer)
        {
            id = HistoryId::Invalid;
            ++bias;
        }
        else
            id = (HistoryId)(i - bias);
        new_ids.push_back(id);
    }
    auto new_id = [&new_ids](HistoryId old_id) -> HistoryId {
        return old_id == HistoryId::Invalid ? HistoryId::Invalid : new_ids[(size_t)old_id];
    };

    m_history.erase(remove_if(m_history, [&buffer](const auto& node) {
        return &node.selections.buffer() == &buffer;
    }), m_history.end());

    for (auto& node : m_history)
    {
        node.parent = new_id(node.parent);
        node.redo_child = new_id(node.redo_child);
    }
    m_history_id = new_id(m_history_id);
    if (m_staging)
    {
        m_staging->parent = new_id(m_staging->parent);
        kak_assert(m_staging->redo_child == HistoryId::Invalid);
    }
    kak_assert(m_history_id != HistoryId::Invalid or m_staging);
}

void Context::change_buffer(Buffer& buffer, Optional<FunctionRef<void()>> set_selections)
{
    if (has_buffer() and &buffer == &this->buffer())
        return;

    if (has_buffer() and m_edition_level > 0)
       this->buffer().commit_undo_group();

    if (has_client())
    {
        client().info_hide();
        client().menu_hide();
        client().change_buffer(buffer, std::move(set_selections));
    }
    else
    {
        m_window.reset();
        if (m_selection_history.empty())
            m_selection_history.initialize(SelectionList{buffer, Selection{}});
        else
        {
            ScopedSelectionEdition selection_edition{*this};
            selections_write_only() = SelectionList{buffer, Selection{}};
        }
        if (not m_local_scopes.empty())
            m_local_scopes.front()->reparent(buffer);
    }

    if (has_input_handler())
        input_handler().reset_normal_mode();
}

void Context::forget_buffer(Buffer& buffer)
{
    m_jump_list.forget_buffer(buffer);

    if (&this->buffer() == &buffer)
    {
        if (is_editing() && has_input_handler())
            input_handler().reset_normal_mode();

        auto last_buffer = this->last_buffer();
        change_buffer(last_buffer ? *last_buffer : BufferManager::instance().get_first_buffer());
    }

    m_selection_history.forget_buffer(buffer);
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

SelectionList& Context::selections(bool update)
{
    return m_selection_history.selections(update);
}

template<Direction direction>
void Context::undo_selection_change()
{
    m_selection_history.undo<direction>();
}
template void Context::undo_selection_change<Backward>();
template void Context::undo_selection_change<Forward>();

SelectionList& Context::selections_write_only()
{
    return selections(false);
}

const SelectionList& Context::selections(bool update) const
{
    return const_cast<Context&>(*this).selections(update);
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
    size_t index = has_buffer() ? selections(false).main_index() : 0;
    return RegisterManager::instance()[reg].get_main(*this, index);
}

void Context::set_name(String name) {
    String old_name = std::exchange(m_name, std::move(name));
    hooks().run_hook(Hook::ClientRenamed, format("{}:{}", old_name, m_name), *this);
}

ScopedEdition::ScopedEdition(Context& context)
    : m_context{context},
      m_buffer{context.has_buffer() ? &context.buffer() : nullptr}
{ if (m_buffer) m_context.begin_edition(); }

ScopedEdition::~ScopedEdition() { if (m_buffer) m_context.end_edition(); }

ScopedSelectionEdition::ScopedSelectionEdition(Context& context)
    : m_context{context},
      m_buffer{not (m_context.flags() & Context::Flags::Draft) and context.has_buffer() ? &context.buffer() : nullptr}
{ if (m_buffer) m_context.m_selection_history.begin_edition(); }

ScopedSelectionEdition::ScopedSelectionEdition(ScopedSelectionEdition&& other) : m_context{other.m_context}, m_buffer{other.m_buffer}
{ other.m_buffer = nullptr; }

ScopedSelectionEdition::~ScopedSelectionEdition() { if (m_buffer) m_context.m_selection_history.end_edition(); }

}
