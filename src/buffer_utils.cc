#include "buffer_utils.hh"

#include "buffer_manager.hh"
#include "event_manager.hh"
#include "file.hh"

#include <unistd.h>
#include <sys/select.h>

#if defined(__APPLE__)
#define st_mtim st_mtimespec
#endif


namespace Kakoune
{

CharCount get_column(const Buffer& buffer,
                     CharCount tabstop, ByteCoord coord)
{
    auto line = buffer[coord.line];
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

ByteCount get_byte_to_column(const Buffer& buffer, CharCount tabstop, CharCoord coord)
{
    auto line = buffer[coord.line];
    auto col = 0_char;
    auto it = line.begin();
    while (it != line.end() and coord.column > col)
    {
        if (*it == '\t')
        {
            col = (col / tabstop + 1) * tabstop;
            if (col > coord.column) // the target column was in the tab
                break;
        }
        else
            ++col;
        it = utf8::next(it, line.end());
    }
    return (int)(it - line.begin());
}

Buffer* open_file_buffer(StringView filename)
{
    MappedFile file_data{filename};
    return new Buffer(filename.str(), Buffer::Flags::File,
                      file_data, file_data.st.st_mtim);
}

Buffer* open_or_create_file_buffer(StringView filename)
{
    if (file_exists(filename))
    {
        MappedFile file_data{filename};
        return new Buffer(filename.str(), Buffer::Flags::File,
                          file_data, file_data.st.st_mtim);
    }
    return new Buffer(filename.str(), Buffer::Flags::File | Buffer::Flags::New,
                      {}, InvalidTime);
}

void reload_file_buffer(Buffer& buffer)
{
    kak_assert(buffer.flags() & Buffer::Flags::File);
    MappedFile file_data{buffer.name()};
    buffer.reload(file_data, file_data.st.st_mtim);
}

Buffer* create_fifo_buffer(String name, int fd, bool scroll)
{
    static ValueId s_fifo_watcher_id = ValueId::get_free_id();

    Buffer* buffer = BufferManager::instance().get_buffer_ifp(name);
    if (buffer)
    {
        buffer->flags() |= Buffer::Flags::NoUndo;
        buffer->reload({}, InvalidTime);
    }
    else
        buffer = new Buffer(std::move(name), Buffer::Flags::Fifo | Buffer::Flags::NoUndo);

    auto watcher_deleter = [buffer](FDWatcher* watcher) {
        kak_assert(buffer->flags() & Buffer::Flags::Fifo);
        watcher->close_fd();
        buffer->run_hook_in_own_context("BufCloseFifo", "");
        buffer->flags() &= ~Buffer::Flags::Fifo;
        delete watcher;
    };

    // capture a non static one to silence a warning.
    ValueId fifo_watcher_id = s_fifo_watcher_id;

    std::unique_ptr<FDWatcher, decltype(watcher_deleter)> watcher(
        new FDWatcher(fd, [buffer, scroll, fifo_watcher_id](FDWatcher& watcher, EventMode mode) {
        if (mode != EventMode::Normal)
            return;

        kak_assert(buffer->flags() & Buffer::Flags::Fifo);

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
            auto pos = buffer->back_coord();

            const bool prevent_scrolling = pos == ByteCoord{0,0} and not scroll;
            if (prevent_scrolling)
                pos = buffer->next(pos);

            buffer->insert(pos, StringView(data, data+count));

            if (count > 0 and prevent_scrolling)
            {
                buffer->erase({0,0}, buffer->next({0,0}));
                // in the other case, the buffer will have automatically
                // inserted a \n to guarantee its invariant.
                if (data[count-1] == '\n')
                    buffer->insert(buffer->end_coord(), "\n");
            }

            FD_ZERO(&rfds);
            FD_SET(fifo, &rfds);
        }
        while (--loops and count > 0 and
               select(fifo+1, &rfds, nullptr, nullptr, &tv) == 1);

        buffer->run_hook_in_own_context("BufReadFifo", buffer->name());

        if (count <= 0)
            buffer->values().erase(fifo_watcher_id); // will delete this
    }), std::move(watcher_deleter));

    buffer->values()[fifo_watcher_id] = Value(std::move(watcher));
    buffer->flags() = Buffer::Flags::Fifo | Buffer::Flags::NoUndo;
    buffer->run_hook_in_own_context("BufOpenFifo", buffer->name());

    return buffer;
}

void write_to_debug_buffer(StringView str)
{
    if (not BufferManager::has_instance())
    {
        write_stderr(str);
        write_stderr("\n");
        return;
    }

    constexpr StringView debug_buffer_name = "*debug*";
    // Try to ensure we keep an empty line at the end of the debug buffer
    // where the user can put its cursor to scroll with new messages
    const bool eol_back = not str.empty() and str.back() == '\n';
    if (Buffer* buffer = BufferManager::instance().get_buffer_ifp(debug_buffer_name))
        buffer->insert(buffer->back_coord(), eol_back ? str : str + "\n");
    else
    {
        String line = str + (eol_back ? "\n" : "\n\n");
        new Buffer(debug_buffer_name.str(), Buffer::Flags::NoUndo | Buffer::Flags::Debug, line, InvalidTime);
    }
}

}
