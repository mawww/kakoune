#ifndef file_hh_INCLUDED
#define file_hh_INCLUDED

#include <string>
#include <stdexcept>

namespace Kakoune
{

struct open_file_error : public std::runtime_error
{
    open_file_error(const std::string& what)
        : std::runtime_error(what) {}
};

struct file_not_found : public open_file_error
{
    file_not_found(const std::string& what)
        : open_file_error(what) {}
};

struct write_file_error : public std::runtime_error
{
    write_file_error(const std::string& what)
        : std::runtime_error(what) {}
};

class Buffer;
Buffer* create_buffer_from_file(const std::string& filename);
void write_buffer_to_file(const Buffer& buffer, const std::string& filename);

}

#endif // file_hh_INCLUDED
