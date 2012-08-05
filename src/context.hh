#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "window.hh"

namespace Kakoune
{

class Buffer;

struct Context
{
    Context()
        : m_window(nullptr), m_buffer(nullptr) {}
    Context(Window& window)
        : m_window(&window), m_buffer(&window.buffer()) {}
    Context(Buffer& buffer)
        : m_window(nullptr), m_buffer(&buffer) {}

    Buffer& buffer() const
    {
        if (not m_buffer)
            throw runtime_error("no buffer in context");
        return *m_buffer;
    }
    bool has_buffer() const { return m_buffer; }

    Window& window() const
    {
        if (not m_window)
            throw runtime_error("no window in context");
        return *m_window;
    }
    bool has_window() const { return m_window; }

    OptionManager& option_manager() const
    {
        if (m_window)
            return m_window->option_manager();
        if (m_buffer)
            return m_buffer->option_manager();
        return GlobalOptionManager::instance();
    }

    int numeric_param() const { return m_numeric_param; }
    void numeric_param(int param) { m_numeric_param = param; }


public:
    safe_ptr<Window> m_window;
    safe_ptr<Buffer> m_buffer;

    int m_numeric_param = 0;
};

}
#endif // context_hh_INCLUDED
