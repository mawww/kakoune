#ifndef file_hh_INCLUDED
#define file_hh_INCLUDED

#include "array_view.hh"
#include "meta.hh"
#include "units.hh"
#include "vector.hh"

#include <sys/types.h>
#include <sys/stat.h>

namespace Kakoune
{

class Buffer;
class String;
class StringView;
class Regex;

using CandidateList = Vector<String, MemoryDomain::Completion>;

// parse ~/ and $env values in filename and returns the translated filename
String parse_filename(StringView filename);
String real_path(StringView filename);
String compact_path(StringView filename);

StringView tmpdir();

// returns pair { directory, filename }
std::pair<StringView, StringView> split_path(StringView path);

String get_kak_binary_path();

bool fd_readable(int fd);
bool fd_writable(int fd);
String read_fd(int fd, bool text = false);
String read_file(StringView filename, bool text = false);
void write(int fd, StringView data);

struct MappedFile
{
    MappedFile(StringView filename);
    ~MappedFile();

    operator StringView() const;

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

void make_directory(StringView dir, mode_t mode);

timespec get_fs_timestamp(StringView filename);

constexpr bool operator==(const timespec& lhs, const timespec& rhs)
{
    return lhs.tv_sec == rhs.tv_sec and lhs.tv_nsec == rhs.tv_nsec;
}

constexpr bool operator!=(const timespec& lhs, const timespec& rhs)
{
    return not (lhs == rhs);
}

enum class FilenameFlags
{
    None = 0,
    OnlyDirectories = 1 << 0,
    Expand = 1 << 1
};
constexpr bool with_bit_ops(Meta::Type<FilenameFlags>) { return true; }

CandidateList complete_filename(StringView prefix, const Regex& ignore_regex,
                                ByteCount cursor_pos = -1,
                                FilenameFlags flags = FilenameFlags::None);

CandidateList complete_command(StringView prefix, ByteCount cursor_pos = -1);

}

#endif // file_hh_INCLUDED
