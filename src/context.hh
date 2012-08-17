#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "window.hh"
#include "client.hh"

namespace Kakoune
{

struct Context
{
    Context() {}
    Context(Editor& editor)
        : m_editor(&editor) {}

    Context(Client& client)
        : m_client(&client) {}

    // to allow func(Context(Editor(...)))
    Context(Editor&& editor)
        : m_editor(&editor) {}

    Context& operator=(const Context&) = delete;

    Buffer& buffer() const
    {
        if (not has_buffer())
            throw runtime_error("no buffer in context");
        return m_editor->buffer();
    }
    bool has_buffer() const { return m_editor; }

    Editor& editor() const
    {
        if (not has_editor())
            throw runtime_error("no editor in context");
        return *m_editor.get();
    }
    bool has_editor() const { return m_editor; }

    Window& window() const
    {
        if (not has_window())
            throw runtime_error("no window in context");
        return *dynamic_cast<Window*>(m_editor.get());
    }
    bool has_window() const { return m_editor and dynamic_cast<Window*>(m_editor.get()); }

    Client& client() const
    {
        if (not has_client())
            throw runtime_error("no client in context");
        return *m_client;
    }
    bool has_client() const { return m_client; }

    void change_editor(Editor& editor)
    {
        m_editor.reset(&editor);
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
        if (has_client() and has_window())
            client().draw_window(window());
    }

    void print_status(const String& status) const
    {
        if (has_client())
            client().print_status(status);
    }

    int numeric_param() const { return m_numeric_param; }
    void numeric_param(int param) { m_numeric_param = param; }

public:
    safe_ptr<Editor> m_editor;
    safe_ptr<Client> m_client;
    int m_numeric_param = 0;
};

}
#endif // context_hh_INCLUDED
