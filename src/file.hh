#ifndef file_hh_INCLUDED
#define file_hh_INCLUDED

#include "array_view.hh"
#include "completion.hh"
#include "exception.hh"
#include "regex.hh"

#include <sys/types.h>
#include <sys/stat.h>

namespace Kakoune
{

struct file_access_error : runtime_error
{
public:
    file_access_error(StringView filename,
                      StringView error_desc)
        : runtime_error(format("{}: {}", filename, error_desc)) {}
};

struct file_not_found : file_access_error
{
    file_not_found(StringView filename)
        : file_access_error(filename, "file not found") {}
};

class Buffer;
class String;
class StringView;

// parse ~/ and $env values in filename and returns the translated filename
String parse_filename(StringView filename);
String real_path(StringView filename);
String compact_path(StringView filename);

// returns pair { directory, filename }
std::pair<StringView, StringView> split_path(StringView path);

String get_kak_binary_path();

String read_fd(int fd, bool text = false);
String read_file(StringView filename, bool text = false);
void write(int fd, StringView data);
inline void write_stdout(StringView str) { write(1, str); }
inline void write_stderr(StringView str) { write(2, str); }


struct MappedFile
{
    MappedFile(StringView filename);
    ~MappedFile();

    operator StringView() const { return { data, (int)st.st_size }; }

    int fd;
    const char* data;
    struct stat st {};
};

void write_buffer_to_file(Buffer& buffer, StringView filename);
void write_buffer_to_fd(Buffer& buffer, int fd);
void write_buffer_to_backup_file(Buffer& buffer);

String find_file(StringView filename, ConstArrayView<String> paths);
bool file_exists(StringView filename);

Vector<String> list_files(StringView directory);

void make_directory(StringView dir);

timespec get_fs_timestamp(StringView filename);

constexpr bool operator==(const timespec& lhs, const timespec& rhs)
{
    return lhs.tv_sec == rhs.tv_sec and lhs.tv_nsec == rhs.tv_nsec;
}

constexpr bool operator!=(const timespec& lhs, const timespec& rhs)
{
    return not (lhs == rhs);
}

CandidateList complete_filename(StringView prefix, const Regex& ignore_regex,
                                ByteCount cursor_pos = -1);

CandidateList complete_command(StringView prefix, ByteCount cursor_pos = -1);

}

#endif // file_hh_INCLUDED
