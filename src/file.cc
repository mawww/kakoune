#include "file.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "assert.hh"

#include "unicode.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>


namespace Kakoune
{

String parse_filename(const String& filename)
{
    if (filename.length() >= 2 and filename[0] == '~' and filename[1] == '/')
        return parse_filename("$HOME/" + filename.substr(2_byte));

    ByteCount pos = 0;
    String result;
    for (ByteCount i = 0; i < filename.length(); ++i)
    {
        if (filename[i] == '$' and (i == 0 or filename[i-1] != '\\'))
        {
            result += filename.substr(pos, i - pos);
            ByteCount end = i+1;
            while (end != filename.length() and is_word(filename[end]))
                ++end;
            String var_name = filename.substr(i+1, end - i - 1);
            const char* var_value = getenv(var_name.c_str());
            if (var_value)
                result += var_value;

            pos = end;
        }
    }
    if (pos != filename.length())
        result += filename.substr(pos);

    return result;
}

String read_file(const String& filename)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            throw file_not_found(filename);

        throw file_access_error(filename, strerror(errno));
    }
    auto close_fd = on_scope_end([fd]{ close(fd); });

    String content;
    char buf[256];
    while (true)
    {
        ssize_t size = read(fd, buf, 256);
        if (size == -1 or size == 0)
            break;

        content += String(buf, buf + size);
    }
    return content;
}

Buffer* create_buffer_from_file(const String& filename)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            return nullptr;

        throw file_access_error(filename, strerror(errno));
    }
    struct stat st;
    fstat(fd, &st);
    if (S_ISDIR(st.st_mode))
    {
        close(fd);
        throw file_access_error(filename, "is a directory");
    }
    const char* data = (const char*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    auto cleanup = on_scope_end([&]{ munmap((void*)data, st.st_size); close(fd); });

    if (Buffer* buffer = BufferManager::instance().get_buffer(filename))
        delete buffer;

    const char* pos = data;
    bool crlf = false;
    bool bom  = false;
    if (st.st_size >= 3 and
       data[0] == '\xEF' and data[1] == '\xBB' and data[2] == '\xBF')
    {
        bom = true;
        pos = data + 3;
    }

    std::vector<String> lines;
    const char* end = data + st.st_size;
    while (pos < end)
    {
        const char* line_end = pos;
        while (line_end < end and *line_end != '\r' and *line_end != '\n')
             ++line_end;

        // this should happen only when opening a file which has no
        // end of line as last character.
        if (line_end == end)
        {
            lines.emplace_back(pos, line_end);
            lines.back() += '\n';
            break;
        }

        lines.emplace_back(pos, line_end + 1);
        lines.back().back() = '\n';

        if (line_end+1 != end and *line_end == '\r' and *(line_end+1) == '\n')
        {
            crlf = true;
            pos = line_end + 2;
        }
        else
            pos = line_end + 1;
    }
    Buffer* buffer = new Buffer(filename, Buffer::Flags::File, std::move(lines));

    OptionManager& options = buffer->options();
    options.set_option("eolformat", Option(crlf ? "crlf" : "lf"));
    options.set_option("BOM", Option(bom ? "utf-8" : "no"));

    return buffer;
}

static void write(int fd, const memoryview<char>& data, const String& filename)
{
    const char* ptr = data.pointer();
    ssize_t count   = data.size();

    while (count)
    {
        ssize_t written = ::write(fd, ptr, count);
        ptr += written;
        count -= written;

        if (written == -1)
            throw file_access_error(filename, strerror(errno));
    }
}

void write_buffer_to_file(const Buffer& buffer, const String& filename)
{
    String eolformat = buffer.options()["eolformat"].as_string();
    if (eolformat == "crlf")
        eolformat = "\r\n";
    else
        eolformat = "\n";
    auto eoldata = eolformat.data();

    int fd = open(parse_filename(filename).c_str(),
                  O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));
    auto close_fd = on_scope_end([fd]{ close(fd); });

    if (buffer.options()["BOM"].as_string() == "utf-8")
        ::write(fd, "\xEF\xBB\xBF", 3);

    for (LineCount i = 0; i < buffer.line_count(); ++i)
    {
        // end of lines are written according to eolformat but always
        // stored as \n
        memoryview<char> linedata = buffer.line_content(i).data();
        write(fd, linedata.subrange(0, linedata.size()-1), filename);
        write(fd, eoldata, filename);
    }
}

String find_file(const String& filename, const memoryview<String>& paths)
{
    for (auto path : paths)
    {
        String candidate = path + filename;
        struct stat buf;
        if (stat(candidate.c_str(), &buf) == 0 and S_ISREG(buf.st_mode))
            return candidate;
    }
    return "";
}

}
