#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "window.hh"

namespace Kakoune
{

class Buffer;

struct Context
{
    Context() {}
    Context(Editor& editor)
        : m_editor(&editor), m_buffer(&editor.buffer()) {}

    // to allow func(Context(Editor(...)))
    Context(Editor&& editor)
        : m_editor(&editor), m_buffer(&editor.buffer()) {}

    Buffer& buffer() const
    {
        if (not has_buffer())
            throw runtime_error("no buffer in context");
        return *m_buffer;
    }
    bool has_buffer() const { return m_buffer; }

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

    OptionManager& option_manager() const
    {
        if (has_window())
            return window().option_manager();
        if (has_buffer())
            return buffer().option_manager();
        return GlobalOptionManager::instance();
    }

    int numeric_param() const { return m_numeric_param; }
    void numeric_param(int param) { m_numeric_param = param; }


public:
    safe_ptr<Editor> m_editor;
    safe_ptr<Buffer> m_buffer;

    int m_numeric_param = 0;
};

}
#endif // context_hh_INCLUDED
