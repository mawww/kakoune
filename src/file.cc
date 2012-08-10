#include "file.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "assert.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>


namespace Kakoune
{

bool isidentifier(char c)
{
    return std::isalnum(c) or c == '_';
}

String parse_filename(const String& filename)
{
    if (filename.length() > 2 and filename[0] == '~' and filename[1] == '/')
        return parse_filename("$HOME/" + filename.substr(2));

    size_t pos = 0;
    String result;
    for (size_t i = 0; i < filename.length(); ++i)
    {
        if (filename[i] == '$' and (i == 0 or filename[i-1] != '\\'))
        {
            result += filename.substr(pos, i - pos);
            size_t end = i+1;
            while (end != filename.length() and isidentifier(filename[end]))
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
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            throw file_not_found(filename);

        throw file_access_error(filename, strerror(errno));
    }

    String content;
    char buf[256];
    while (true)
    {
        ssize_t size = read(fd, buf, 256);
        if (size == -1 or size == 0)
            break;

        content += String(buf, buf + size);
    }
    close(fd);
    return content;
}

Buffer* create_buffer_from_file(const String& filename)
{
    if (Buffer* buffer = BufferManager::instance().get_buffer(filename))
        delete buffer;

    Buffer* buffer = new Buffer(filename, Buffer::Type::File, "");

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            throw file_not_found(filename);

        throw file_access_error(filename, strerror(errno));
    }

    String content;
    char buf[256];
    bool crlf = false;
    bool bom  = false;
    bool at_file_begin = true;
    while (true)
    {
        ssize_t size = read(fd, buf, 256);
        if (size == -1 or size == 0)
            break;

        ssize_t pos = 0;
        // detect utf-8 byte order mark
        if (at_file_begin and size >= 3 and
            buf[0] == '\xEF' and buf[1] == '\xBB' and buf[2] == '\xBF')
        {
            bom = true;
            pos = 3;
        }
        ssize_t start = pos;

        while (pos < size+1)
        {
            if (buf[pos] == '\r' or pos == size)
            {
                if (buf[pos] == '\r')
                    crlf = true;

                buffer->modify(Modification::make_insert(buffer->end(), String(buf+start, buf+pos)));
                start = pos+1;
            }
            ++pos;
        }
        at_file_begin = false;
    }
    close(fd);

    OptionManager& option_manager = buffer->option_manager();
    option_manager.set_option("eolformat", Option(crlf ? "crlf" : "lf"));
    option_manager.set_option("BOM", Option(bom ? "utf-8" : "no"));

    // it never happened, buffer always was like that
    buffer->reset_undo_data();

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
    String eolformat = buffer.option_manager()["eolformat"].as_string();
    if (eolformat == "crlf")
        eolformat = "\r\n";
    else
        eolformat = "\n";
    auto eoldata = eolformat.data();

    int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));

    if (buffer.option_manager()["BOM"].as_string() == "utf-8")
        ::write(fd, "\xEF\xBB\xBF", 3);

    for (size_t i = 0; i < buffer.line_count(); ++i)
    {
        // end of lines are written according to eolformat but always
        // stored as \n
        memoryview<char> linedata = buffer.line_content(i).data();
        write(fd, linedata.subrange(0, linedata.size()-1), filename);
        write(fd, eoldata, filename);
    }
    close(fd);
}

}
