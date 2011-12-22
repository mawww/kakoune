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

std::string read_file(const std::string& filename)
{
   int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            throw file_not_found(filename);

        throw file_access_error(filename, strerror(errno));
    }

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
    return content;
}

Buffer* create_buffer_from_file(const std::string& filename)
{
    std::string content = read_file(filename);

    if (Buffer* buffer = BufferManager::instance().get_buffer(filename))
        BufferManager::instance().delete_buffer(buffer);

    return new Buffer(filename, Buffer::Type::File, content);
}

void write_buffer_to_file(const Buffer& buffer, const std::string& filename)
{
    int fd = open(filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));

    const BufferString& content = buffer.content();
    ssize_t count = content.length() * sizeof(BufferChar);
    const char* ptr = content.c_str();

    while (count)
    {
        ssize_t written = write(fd, ptr, count);
        ptr += written;
        count -= written;

        if (written == -1)
            throw file_access_error(filename, strerror(errno));
    }
    close(fd);
}

}
