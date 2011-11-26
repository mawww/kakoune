#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "window.hh"

namespace Kakoune
{

struct Context
{
    Window* window;
    Buffer* buffer;

    Context()
        : window(nullptr), buffer(nullptr) {}
    Context(Window& window)
        : window(&window), buffer(&window.buffer()) {}
    Context(Buffer& buffer)
        : window(nullptr), buffer(&buffer) {}
};

}
#endif // context_hh_INCLUDED
