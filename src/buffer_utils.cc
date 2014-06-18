#include "buffer_utils.hh"

#include "event_manager.hh"

#include <sys/select.h>

namespace Kakoune
{

CharCount get_column(const Buffer& buffer,
                     CharCount tabstop, ByteCoord coord)
{
    auto& line = buffer[coord.line];
    auto col = 0_char;
    for (auto it = line.begin();
         it != line.end() and coord.column > (int)(it - line.begin());
         it = utf8::next(it))
    {
        if (*it == '\t')
            col = (col / tabstop + 1) * tabstop;
        else
            ++col;
    }
    return col;
}

Buffer* create_fifo_buffer(String name, int fd, bool scroll)
{
    Buffer* buffer = new Buffer(std::move(name), Buffer::Flags::Fifo | Buffer::Flags::NoUndo);

    auto watcher = new FDWatcher(fd, [buffer, scroll](FDWatcher& watcher) {
        constexpr size_t buffer_size = 2048;
        char data[buffer_size];
        const int fifo = watcher.fd();
        timeval tv{ 0, 0 };
        fd_set  rfds;
        ssize_t count = 0;
        do
        {
            count = read(fifo, data, buffer_size);
            auto pos = buffer->end()-1;

            bool prevent_scrolling = pos == buffer->begin() and not scroll;
            if (prevent_scrolling)
                ++pos;

            buffer->insert(pos, count > 0 ? String(data, data+count)
                                          : "*** kak: fifo closed ***\n");

            if (prevent_scrolling)
            {
                buffer->erase(buffer->begin(), buffer->begin()+1);
                buffer->insert(buffer->end(), "\n");
            }

            FD_ZERO(&rfds);
            FD_SET(fifo, &rfds);
        }
        while (count > 0 and select(fifo+1, &rfds, nullptr, nullptr, &tv) == 1);

        if (count <= 0)
        {
            kak_assert(buffer->flags() & Buffer::Flags::Fifo);
            buffer->flags() &= ~Buffer::Flags::Fifo;
            buffer->flags() &= ~Buffer::Flags::NoUndo;
            close(fifo);
            buffer->run_hook_in_own_context("BufCloseFifo", "");
            delete &watcher;
        }
    });

    buffer->hooks().add_hook("BufClose", "",
        [buffer, watcher](const String&, const Context&) {
            // Check if fifo is still alive, else watcher is already dead
            if (buffer->flags() & Buffer::Flags::Fifo)
            {
                close(watcher->fd());
                buffer->run_hook_in_own_context("BufCloseFifo", "");
                delete watcher;
            }
        });

    return buffer;
}

}
