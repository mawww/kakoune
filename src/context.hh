#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "window.hh"
#include "client.hh"
#include "user_interface.hh"

namespace Kakoune
{

// A Context is used to access non singleton objects for various services
// in commands.
//
// The Context object links a Client, an Editor (which may be a Window),
// and a UserInterface. It may represent an interactive user window, or
// a hook execution or a macro replay.
struct Context
{
    Context() {}
    explicit Context(Editor& editor)
        : m_editor(&editor) {}

    explicit Context(Client& client)
        : m_client(&client) {}

    // to allow func(Context(Editor(...)))
    // make sure the context will not survive the next ';'
    explicit Context(Editor&& editor)
        : m_editor(&editor) {}

    Context(const Context&) = delete;
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

    Client& client() const
    {
        if (not has_client())
            throw runtime_error("no client in context");
        return *m_client;
    }
    bool has_client() const { return (bool)m_client; }

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
    }

    void change_ui(UserInterface& ui)
    {
        m_ui.reset(&ui);
    }

    OptionManager& option_manager() const
    {
        if (has_window())
            return window().option_manager();
        if (has_buffer())
            return buffer().option_manager();
        return GlobalOptionManager::instance();
    }

    void draw_ifn() const
    {
        if (has_ui() and has_window())
            ui().draw_window(window());
    }

    void print_status(const String& status) const
    {
        if (has_ui())
            ui().print_status(status);
    }

    using Insertion = std::pair<InsertMode, std::vector<Key>>;
    Insertion& last_insert() { return m_last_insert; }

    int& numeric_param() { return m_numeric_param; }
private:
    safe_ptr<Editor>        m_editor;
    safe_ptr<Client>        m_client;
    safe_ptr<UserInterface> m_ui;

    Insertion m_last_insert = {InsertMode::Insert, {}};
    int m_numeric_param = 0;
};

}
#endif // context_hh_INCLUDED
