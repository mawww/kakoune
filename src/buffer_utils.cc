#include "buffer_utils.hh"

#include "buffer_manager.hh"
#include "event_manager.hh"
#include "file.hh"

#include <unistd.h>
#include <sys/select.h>

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

struct BufferData
{
    BufferLines lines;
    bool bom = false;
    bool crlf = false;

    BufferData(StringView data)
    {
        const char* pos = data.begin();
        if (data.length() >= 3 and
           data[0_byte] == '\xEF' and data[1_byte] == '\xBB' and data[2_byte] == '\xBF')
        {
            bom = true;
            pos = data.begin() + 3;
        }

        while (pos < data.end())
        {
            const char* line_end = pos;
            while (line_end < data.end() and *line_end != '\r' and *line_end != '\n')
                 ++line_end;

            lines.emplace_back(StringData::create({pos, line_end}, '\n'));

            if (line_end+1 != data.end() and *line_end == '\r' and *(line_end+1) == '\n')
            {
                crlf = true;
                pos = line_end + 2;
            }
            else
                pos = line_end + 1;
        }
    }

    void apply_options(Buffer& buffer) const
    {
        OptionManager& options = buffer.options();
        options.get_local_option("eolformat").set<String>(crlf ? "crlf" : "lf");
        options.get_local_option("BOM").set<String>(bom ? "utf-8" : "no");
    }
};

Buffer* create_file_buffer(StringView filename)
{
    if (MappedFile file_data{filename})
        return create_buffer({ file_data.data, (int)file_data.st.st_size }, filename,
                             Buffer::Flags::File, file_data.st.st_mtim);
    return nullptr;
}

bool reload_file_buffer(Buffer& buffer)
{
    kak_assert(buffer.flags() & Buffer::Flags::File);
    if (MappedFile file_data{buffer.name()})
    {
        reload_buffer(buffer,  { file_data.data, (int)file_data.st.st_size }, file_data.st.st_mtim);
        return true;
    }
    return false;
}

Buffer* create_buffer(StringView data, StringView name, Buffer::Flags flags,
                      timespec fs_timestamp)
{
    BufferData buf_data(data);
    Buffer* buffer = new Buffer{name.str(), flags, std::move(buf_data.lines), fs_timestamp};
    buf_data.apply_options(*buffer);
    return buffer;
}

void reload_buffer(Buffer& buffer, StringView data, timespec fs_timestamp)
{
    BufferData buf_data(data);
    buffer.reload(std::move(buf_data.lines), fs_timestamp);
    buf_data.apply_options(buffer);
}

Buffer* create_fifo_buffer(String name, int fd, bool scroll)
{
    static ValueId s_fifo_watcher_id = ValueId::get_free_id();

    Buffer* buffer = BufferManager::instance().get_buffer_ifp(name);
    if (buffer)
    {
        buffer->flags() |= Buffer::Flags::NoUndo;
        buffer->reload({"\n"_ss}, InvalidTime);
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
            auto pos = buffer->end()-1;

            bool prevent_scrolling = pos == buffer->begin() and not scroll;
            if (prevent_scrolling)
                ++pos;

            buffer->insert(pos, StringView(data, data+count));

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
        write(2, str.data(), (int)str.length());
        write(2, "\n", 1);
        return;
    }

    const StringView debug_buffer_name = "*debug*";
    if (Buffer* buffer = BufferManager::instance().get_buffer_ifp(debug_buffer_name))
        buffer->insert(buffer->end(), str);
    else
    {
        String line = str + ((str.empty() or str.back() != '\n') ? "\n" : "");
        create_buffer(line, debug_buffer_name, Buffer::Flags::NoUndo, InvalidTime);
    }
}

}
