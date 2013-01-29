#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "window.hh"
#include "user_interface.hh"

namespace Kakoune
{

class InputHandler;

// A Context is used to access non singleton objects for various services
// in commands.
//
// The Context object links an InputHandler, an Editor (which may be a Window),
// and a UserInterface. It may represent an interactive user window, or
// a hook execution or a macro replay.
struct Context
{
    Context() {}
    explicit Context(Editor& editor)
        : m_editor(&editor) {}
    Context(InputHandler& input_handler, UserInterface& ui)
        : m_input_handler(&input_handler), m_ui(&ui) {}
    Context(InputHandler& input_handler, Editor& editor, UserInterface& ui)
        : m_input_handler(&input_handler), m_editor(&editor), m_ui(&ui) {}

    // to allow func(Context(Editor(...)))
    // make sure the context will not survive the next ';'
    explicit Context(Editor&& editor)
        : m_editor(&editor) {}

    Context(const Context&) = delete;
    Context(Context&&) = default;
    Context& operator=(const Context&) = delete;

    Buffer& buffer() const
    {
        if (not has_buffer())
            throw runtime_error("no buffer in context");
        return m_editor->buffer();
    }
    bool has_buffer() const { return (bool)m_editor; }

    Editor& editor() const
    {
        if (not has_editor())
            throw runtime_error("no editor in context");
        return *m_editor.get();
    }
    bool has_editor() const { return (bool)m_editor; }

    Window& window() const
    {
        if (not has_window())
            throw runtime_error("no window in context");
        return *dynamic_cast<Window*>(m_editor.get());
    }
    bool has_window() const { return (bool)m_editor and dynamic_cast<Window*>(m_editor.get()); }

    InputHandler& input_handler() const
    {
        if (not has_input_handler())
            throw runtime_error("no input handler in context");
        return *m_input_handler;
    }
    bool has_input_handler() const { return (bool)m_input_handler; }

    UserInterface& ui() const
    {
        if (not has_ui())
            throw runtime_error("no user interface in context");
        return *m_ui;
    }
    bool has_ui() const { return (bool)m_ui; }

    void change_editor(Editor& editor)
    {
        m_editor.reset(&editor);
        if (has_window() && has_ui())
            window().set_dimensions(ui().dimensions());
    }

    OptionManager& options() const
    {
        if (has_window())
            return window().options();
        if (has_buffer())
            return buffer().options();
        return GlobalOptions::instance();
    }

    HookManager& hooks() const
    {
        if (has_window())
            return window().hooks();
        if (has_buffer())
            return buffer().hooks();
        return GlobalHooks::instance();
    }

    void print_status(const String& status) const
    {
        if (has_ui())
            ui().print_status(status);
    }

    using Insertion = std::pair<InsertMode, std::vector<Key>>;
    Insertion& last_insert() { return m_last_insert; }

    void push_jump()
    {
        const SelectionList& jump = editor().selections();
        if (m_current_jump != m_jump_list.end())
        {
            auto begin = m_current_jump;
            if (&editor().buffer() != &begin->buffer() or
                (const SelectionList&)(*begin) != jump)
                ++begin;
            m_jump_list.erase(begin, m_jump_list.end());
        }

        m_jump_list.push_back({editor().buffer(), jump});
        m_current_jump = m_jump_list.end();
    }

    const SelectionList& jump_forward()
    {
        if (m_current_jump != m_jump_list.end() and
            m_current_jump + 1 != m_jump_list.end())
            return *++m_current_jump;
        throw runtime_error("no next jump");
    }

    const SelectionList& jump_backward()
    {
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

    void forget_jumps_to_buffer(Buffer& buffer)
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

    int& numeric_param() { return m_numeric_param; }
private:
    safe_ptr<Editor>        m_editor;
    InputHandler*           m_input_handler = nullptr;
    safe_ptr<UserInterface> m_ui;

    Insertion m_last_insert = {InsertMode::Insert, {}};
    int m_numeric_param = 0;

    using JumpList = std::vector<DynamicSelectionList>;
    JumpList           m_jump_list;
    JumpList::iterator m_current_jump = m_jump_list.begin();
};

}
#endif // context_hh_INCLUDED
