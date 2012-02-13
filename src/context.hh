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
public:
    Window* m_window;
    Buffer* m_buffer;

};

}
#endif // context_hh_INCLUDED
