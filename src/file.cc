#include "file.hh"

#include "buffer.hh"
#include "buffer_manager.hh"
#include "assert.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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
    String content = read_file(filename);

    if (Buffer* buffer = BufferManager::instance().get_buffer(filename))
        delete buffer;

    return new Buffer(filename, Buffer::Type::File, content);
}

void write_buffer_to_file(const Buffer& buffer, const String& filename)
{
    int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));

    for (size_t i = 0; i < buffer.line_count(); ++i)
    {
        const String& content = buffer.line_content(i);
        const char* ptr = content.c_str();
        ssize_t count   = content.size();

        while (count)
        {
            ssize_t written = write(fd, ptr, count);
            ptr += written;
            count -= written;

            if (written == -1)
                throw file_access_error(filename, strerror(errno));
        }
    }
    close(fd);
}

}
