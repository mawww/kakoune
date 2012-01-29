#ifndef file_hh_INCLUDED
#define file_hh_INCLUDED

#include <string>

#include "exception.hh"

namespace Kakoune
{

struct file_access_error : runtime_error
{
public:
    file_access_error(const std::string& filename,
                      const std::string& error_desc)
        : runtime_error(filename + ": " + error_desc) {}
};

struct file_not_found : file_access_error
{
    file_not_found(const std::string& filename)
        : file_access_error(filename, "file not found") {}
};

class Buffer;

// parse ~/ and $env values in filename and returns the translated filename
std::string parse_filename(const std::string& filename);

std::string read_file(const std::string& filename);
Buffer* create_buffer_from_file(const std::string& filename);
void write_buffer_to_file(const Buffer& buffer, const std::string& filename);

}

#endif // file_hh_INCLUDED
