#include "buffer_utils.hh"

#include "buffer_manager.hh"
#include "event_manager.hh"
#include "file.hh"
#include "selection.hh"
#include "changes.hh"

#include <unistd.h>

#if defined(__APPLE__)
#define st_mtim st_mtimespec
#endif


namespace Kakoune
{

void replace(Buffer& buffer, ArrayView<BufferRange> ranges, ConstArrayView<String> strings)
{
    ForwardChangesTracker changes_tracker;
    size_t timestamp  = buffer.timestamp();
    for (size_t index = 0; index < ranges.size(); ++index)
    {
        auto& range = ranges[index];
        range.begin = changes_tracker.get_new_coord_tolerant(range.begin);
        range.end = changes_tracker.get_new_coord_tolerant(range.end);
        kak_assert(buffer.is_valid(range.begin) and buffer.is_valid(range.end));

        range = buffer.replace(range.begin, range.end, strings.empty() ? StringView{} : strings[std::min(index, strings.size()-1)]);
        kak_assert(buffer.is_valid(range.begin) and buffer.is_valid(range.end));
        changes_tracker.update(buffer, timestamp);
    }

    buffer.check_invariant();
}

ColumnCount get_column(const Buffer& buffer,
                       ColumnCount tabstop, BufferCoord coord)
{
    auto line = buffer[coord.line];
    auto col = 0_col;
    for (auto it = line.begin();
         it != line.end() and coord.column > (int)(it - line.begin()); )
    {
        if (*it == '\t')
        {
            col = (col / tabstop + 1) * tabstop;
            ++it;
        }
        else
            col += codepoint_width(utf8::read_codepoint(it, line.end()));
    }
    return col;
}

ColumnCount column_length(const Buffer& buffer, ColumnCount tabstop, LineCount line)
{
    return get_column(buffer, tabstop, BufferCoord{line, ByteCount{INT_MAX}});
}

ByteCount get_byte_to_column(const Buffer& buffer, ColumnCount tabstop, DisplayCoord coord)
{
    auto line = buffer[coord.line];
    auto col = 0_col;
    auto it = line.begin();
    while (it != line.end() and coord.column > col)
    {
        if (*it == '\t')
        {
            col = (col / tabstop + 1) * tabstop;
            if (col > coord.column) // the target column was in the tab
                break;
            ++it;
        }
        else
        {
            auto next = it;
            col += codepoint_width(utf8::read_codepoint(next, line.end()));
            if (col > coord.column) // the target column was in the char
                break;
            it = next;
        }
    }
    return (int)(it - line.begin());
}

static BufferLines parse_lines(const char* pos, const char* end, EolFormat eolformat)
{
    BufferLines lines;
    while (pos < end)
    {
        if (lines.size() >= std::numeric_limits<int>::max())
            throw runtime_error("too many lines");

        const char* eol = std::find(pos, end, '\n');
        if ((eol - pos) >= std::numeric_limits<int>::max())
            throw runtime_error("line is too long");

        lines.emplace_back(StringData::create({{pos, eol - (eolformat == EolFormat::Crlf and eol != end ? 1 : 0)}, "\n"}));
        pos = eol + 1;
    }

    if (lines.empty())
        lines.emplace_back(StringData::create({"\n"}));

    return lines;
}

Buffer* create_buffer_from_string(String name, Buffer::Flags flags, StringView data)
{
    return BufferManager::instance().create_buffer(
        std::move(name), flags,
        parse_lines(data.begin(), data.end(), EolFormat::Lf),
        ByteOrderMark::None, EolFormat::Lf,
        FsStatus{InvalidTime, {}, {}});
}

template<typename Func>
decltype(auto) parse_file(StringView filename, Func&& func)
{
    MappedFile file{parse_filename(filename)};

    const char* pos = file.data;
    const char* end = pos + file.st.st_size;

    auto bom = ByteOrderMark::None;
    if (file.st.st_size >= 3 && StringView{pos, 3_byte} == "\xEF\xBB\xBF")
    {
        bom = ByteOrderMark::Utf8;
        pos += 3;
    }

    bool has_crlf = false, has_lf = false;
    for (auto it = pos; it != end; ++it)
    {
        if (*it == '\n')
            ((it != pos and *(it-1) == '\r') ? has_crlf : has_lf) = true;
    }
    const bool crlf = has_crlf and not has_lf;
    auto eolformat = crlf ? EolFormat::Crlf : EolFormat::Lf;

    FsStatus fs_status{file.st.st_mtim, file.st.st_size, hash_data(file.data, file.st.st_size)};
    return func(parse_lines(pos, end, eolformat), bom, eolformat, fs_status);
}

Buffer* open_file_buffer(StringView filename, Buffer::Flags flags)
{
    return parse_file(filename, [&](BufferLines&& lines, ByteOrderMark bom, EolFormat eolformat, FsStatus fs_status)  {
        return BufferManager::instance().create_buffer(filename.str(), Buffer::Flags::File | flags,
                                                       std::move(lines), bom, eolformat, fs_status);
    });
}

Buffer* open_or_create_file_buffer(StringView filename, Buffer::Flags flags)
{
    auto path = parse_filename(filename);
    if (file_exists(path))
        return open_file_buffer(filename.str(), Buffer::Flags::File | flags);
    return create_buffer_from_string(filename.str(), Buffer::Flags::File | Buffer::Flags::New, StringView{});
}

void reload_file_buffer(Buffer& buffer)
{
    kak_assert(buffer.flags() & Buffer::Flags::File);
    parse_file(buffer.name(), [&](auto&&... params) {
        buffer.reload(std::forward<decltype(params)>(params)...);
    });
    buffer.flags() &= ~Buffer::Flags::New;
}

Buffer* create_fifo_buffer(String name, int fd, Buffer::Flags flags, bool scroll)
{
    static ValueId fifo_watcher_id = get_free_value_id();

    auto& buffer_manager = BufferManager::instance();
    Buffer* buffer = buffer_manager.get_buffer_ifp(name);
    if (buffer)
    {
        buffer->flags() |= Buffer::Flags::NoUndo | flags;
        buffer->reload({StringData::create({"\n"})}, ByteOrderMark::None, EolFormat::Lf, {InvalidTime, {}, {}});
    }
    else
        buffer = buffer_manager.create_buffer(
            std::move(name), flags | Buffer::Flags::Fifo | Buffer::Flags::NoUndo,
            {StringData::create({"\n"})}, ByteOrderMark::None, EolFormat::Lf, {InvalidTime, {}, {}});

    struct FifoWatcher : FDWatcher
    {
        FifoWatcher(int fd, Buffer& buffer, bool scroll)
            : FDWatcher(fd, FdEvents::Read, EventMode::Normal,
                        [](FDWatcher& watcher, FdEvents, EventMode mode) {
                            if (mode == EventMode::Normal)
                                static_cast<FifoWatcher&>(watcher).read_fifo();
                        }),
              m_buffer(buffer), m_scroll(scroll)
        {}

        ~FifoWatcher()
        {
            kak_assert(m_buffer.flags() & Buffer::Flags::Fifo);
            close_fd();
            m_buffer.run_hook_in_own_context(Hook::BufCloseFifo, "");
            m_buffer.flags() &= ~(Buffer::Flags::Fifo | Buffer::Flags::NoUndo);
        }

        void read_fifo() const
        {
            kak_assert(m_buffer.flags() & Buffer::Flags::Fifo);

            constexpr size_t buffer_size = 2048;
            // if we read data slower than it arrives in the fifo, limiting the
            // iteration number allows us to go back go back to the event loop and
            // handle other events sources (such as input)
            constexpr size_t max_loop = 1024;
            bool closed = false;
            size_t loop = 0;
            char data[buffer_size];
            BufferCoord insert_coord = m_buffer.back_coord();
            const int fifo = fd();

            {
                auto restore_flags = on_scope_end([this, flags=m_buffer.flags()] { m_buffer.flags() = flags; });
                m_buffer.flags() &= ~Buffer::Flags::ReadOnly;
                do
                {

                    const ssize_t count = ::read(fifo, data, buffer_size);
                    if (count <= 0)
                    {
                        closed = true;
                        break;
                    }

                    auto pos = m_buffer.back_coord();
                    const bool prevent_scrolling = pos == BufferCoord{0,0} and not m_scroll;
                    if (prevent_scrolling)
                        pos = m_buffer.next(pos);

                    m_buffer.insert(pos, StringView(data, data+count));

                    if (prevent_scrolling)
                    {
                        m_buffer.erase({0,0}, m_buffer.next({0,0}));
                        // in the other case, the buffer will have automatically
                        // inserted a \n to guarantee its invariant.
                        if (data[count-1] == '\n')
                            m_buffer.insert(m_buffer.end_coord(), "\n");
                    }
                }
                while (++loop < max_loop  and fd_readable(fifo));
            }

            if (insert_coord != m_buffer.back_coord())
                m_buffer.run_hook_in_own_context(
                    Hook::BufReadFifo,
                    selection_to_string(ColumnType::Byte, m_buffer, {insert_coord, m_buffer.back_coord()}));

            if (closed)
                m_buffer.values().erase(fifo_watcher_id); // will delete this
        }

        Buffer& m_buffer;
        bool m_scroll;
    };

    buffer->values()[fifo_watcher_id] = Value(Meta::Type<FifoWatcher>{}, fd, *buffer, scroll);
    buffer->flags() = flags | Buffer::Flags::Fifo | Buffer::Flags::NoUndo;
    buffer->run_hook_in_own_context(Hook::BufOpenFifo, buffer->name());

    return buffer;
}

void write_to_debug_buffer(StringView str)
{
    if (not BufferManager::has_instance())
    {
        write(2, str);
        write(2, "\n");
        return;
    }

    constexpr StringView debug_buffer_name = "*debug*";
    // Try to ensure we keep an empty line at the end of the debug buffer
    // where the user can put its cursor to scroll with new messages
    const bool eol_back = not str.empty() and str.back() == '\n';
    if (Buffer* buffer = BufferManager::instance().get_buffer_ifp(debug_buffer_name))
    {
        buffer->flags() &= ~Buffer::Flags::ReadOnly;
        auto restore = on_scope_end([buffer] { buffer->flags() |= Buffer::Flags::ReadOnly; });

        buffer->insert(buffer->back_coord(), eol_back ? str : str + "\n");
    }
    else
    {
        String line = str + (eol_back ? "\n" : "\n\n");
        create_buffer_from_string(
            debug_buffer_name.str(), Buffer::Flags::NoUndo | Buffer::Flags::Debug | Buffer::Flags::ReadOnly,
            line);
    }
}


static String modification_as_string(const Buffer::Modification& modification)
{
    return format("{}{}.{}|{}",
                  modification.type == Buffer::Modification::Type::Insert ? '+' : '-',
                  modification.coord.line, modification.coord.column,
                  modification.content->strview());
}

Vector<String> history_as_strings(const Vector<Buffer::HistoryNode>& history)
{
    Vector<String> res;
    for (auto& node : history)
    {
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(node.committed.time_since_epoch());
        res.push_back(to_string(seconds.count()));
        for (auto& modification : node.undo_group)
            res.push_back(modification_as_string(modification));
    };
    return res;
}

Vector<String> undo_group_as_strings(const Buffer::UndoGroup& undo_group)
{
    Vector<String> res;
    for (auto& modification : undo_group)
        res.push_back(modification_as_string(modification));
    return res;
}

String generate_buffer_name(StringView pattern)
{
    auto& buffer_manager = BufferManager::instance();
    for (int i = 0; true; ++i)
    {
        String name = format(pattern, i);
        if (buffer_manager.get_buffer_ifp(name) == nullptr)
            return name;
    }
}

}
