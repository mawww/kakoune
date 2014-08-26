#include "buffer_utils.hh"

#include "buffer_manager.hh"
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
         it = utf8::next(it, line.end()))
    {
        if (*it == '\t')
            col = (col / tabstop + 1) * tabstop;
        else
            ++col;
    }
    return col;
}

Buffer* create_buffer_from_data(StringView data, StringView name,
                                Buffer::Flags flags, time_t fs_timestamp)
{
    bool bom = false, crlf = false;

    const char* pos = data.begin();
    if (data.length() >= 3 and
       data[0] == '\xEF' and data[1] == '\xBB' and data[2] == '\xBF')
    {
        bom = true;
        pos = data.begin() + 3;
    }

    std::vector<String> lines;
    while (pos < data.end())
    {
        const char* line_end = pos;
        while (line_end < data.end() and *line_end != '\r' and *line_end != '\n')
             ++line_end;

        // this should happen only when opening a file which has no
        // end of line as last character.
        if (line_end == data.end())
        {
            lines.emplace_back(pos, line_end);
            lines.back() += '\n';
            break;
        }

        lines.emplace_back(pos, line_end + 1);
        lines.back().back() = '\n';

        if (line_end+1 != data.end() and *line_end == '\r' and *(line_end+1) == '\n')
        {
            crlf = true;
            pos = line_end + 2;
        }
        else
            pos = line_end + 1;
    }

    Buffer* buffer = BufferManager::instance().get_buffer_ifp(name);
    if (buffer)
        buffer->reload(std::move(lines), fs_timestamp);
    else
        buffer = new Buffer{name, flags, std::move(lines), fs_timestamp};

    OptionManager& options = buffer->options();
    options.get_local_option("eolformat").set<String>(crlf ? "crlf" : "lf");
    options.get_local_option("BOM").set<String>(bom ? "utf-8" : "no");

    return buffer;
}

Buffer* create_fifo_buffer(String name, int fd, bool scroll)
{
    Buffer* buffer = new Buffer(std::move(name), Buffer::Flags::Fifo | Buffer::Flags::NoUndo);

    auto watcher = new FDWatcher(fd, [buffer, scroll](FDWatcher& watcher) {
        constexpr size_t buffer_size = 2048;
        // if we read data slower than it arrives in the fifo, limiting the
        // iteration number allows us to go back go back to the event loop and
        // handle other events sources (such as input)
        size_t loops = 16;
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

            buffer->insert(pos, String(data, data+count));

            if (count > 0 and prevent_scrolling)
            {
                buffer->erase(buffer->begin(), buffer->begin()+1);
                // in the other case, the buffer will have automatically
                // inserted a \n to guarantee its invariant.
                if (data[count-1] == '\n')
                    buffer->insert(buffer->end(), "\n");
            }

            FD_ZERO(&rfds);
            FD_SET(fifo, &rfds);
        }
        while (--loops and count > 0 and
               select(fifo+1, &rfds, nullptr, nullptr, &tv) == 1);

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
