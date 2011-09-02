#include "file.hh"
#include "buffer.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <cassert>

namespace Kakoune
{

Buffer* create_buffer_from_file(const std::string& filename)
{
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
        throw open_file_error(strerror(errno));

    std::string content;
    char buf[256];
    while (true)
    {
        ssize_t size = read(fd, buf, 256);
        if (size == -1 or size == 0)
            break;

        content += std::string(buf, size);
    }
    close(fd);
    Buffer* buffer = new Buffer(filename);
    buffer->insert(buffer->begin(), content);
    return buffer;
}

void write_buffer_to_file(const Buffer& buffer, const std::string& filename)
{
    int fd = open(filename.c_str(), O_CREAT | O_WRONLY, 0644);
    if (fd == -1)
        throw open_file_error(strerror(errno));

    const BufferString& content = buffer.content();
    ssize_t count = content.length() * sizeof(BufferChar);
    const char* ptr = content.c_str();

    while (count)
    {
        ssize_t written = write(fd, ptr, count);
        ptr += written;
        count -= written;

        if (written == -1)
            throw write_file_error(strerror(errno));
    }
    close(fd);
}

}
