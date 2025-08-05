#ifndef file_hh_INCLUDED
#define file_hh_INCLUDED

#include "array.hh"
#include "array_view.hh"
#include "enum.hh"
#include "exception.hh"
#include "meta.hh"
#include "string.hh"
#include "units.hh"
#include "utils.hh"

#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <exception>

namespace Kakoune
{

struct file_access_error : runtime_error
{
    file_access_error(StringView filename, StringView error_desc);
    file_access_error(int fd, StringView error_desc);
};

// parse ~/ and %/ in filename and returns the translated filename
String parse_filename(StringView filename, StringView buf_dir = {});

String real_path(StringView filename);
String compact_path(StringView filename);

StringView tmpdir();
StringView homedir();

// returns pair { directory, filename }
std::pair<StringView, StringView> split_path(StringView path);

String get_kak_binary_path();

bool fd_readable(int fd);
bool fd_writable(int fd);
String read_fd(int fd, bool text = false);
String read_file(StringView filename, bool text = false);
template<bool force_blocking = false>
void write(int fd, StringView data);
void write_to_file(StringView filename, StringView data);
int create_file(const char* filename);
int open_temp_file(StringView filename);
int open_temp_file(StringView filename, char (&buffer)[PATH_MAX]);

struct MappedFile
{
    MappedFile(StringView filename);
    ~MappedFile();

    operator StringView() const;

    const char* data;
    struct stat st {};
};

enum class WriteMethod
{
    Overwrite,
    Replace
};
constexpr auto enum_desc(Meta::Type<WriteMethod>)
{
    return make_array<EnumDesc<WriteMethod>>({
        { WriteMethod::Overwrite, "overwrite" },
        { WriteMethod::Replace, "replace" },
    });
}

String find_file(StringView filename, StringView buf_dir, ConstArrayView<String> paths);
bool file_exists(StringView filename);
bool regular_file_exists(StringView filename);

void list_files(StringView directory, FunctionRef<void (StringView, const struct stat&)> callback);

void make_directory(StringView dir, mode_t mode);

constexpr timespec InvalidTime = { -1, -1 };

struct FsStatus
{
    timespec timestamp;
    ByteCount file_size;
    size_t hash;
};

timespec get_fs_timestamp(StringView filename);
FsStatus get_fs_status(StringView filename);

constexpr bool operator==(const timespec& lhs, const timespec& rhs)
{
    return lhs.tv_sec == rhs.tv_sec and lhs.tv_nsec == rhs.tv_nsec;
}

template<bool atomic, int buffer_size = 4096>
struct BufferedWriter
{
    BufferedWriter(int fd)
      : m_fd{fd}, m_exception_count{std::uncaught_exceptions()} {}

    ~BufferedWriter() noexcept(false)
    {
        if (m_pos != 0 and m_exception_count == std::uncaught_exceptions())
            flush();
    }

    void write(StringView data)
    {
        while (not data.empty())
        {
            const ByteCount length = data.length();
            const ByteCount write_len = clamp(length, ByteCount{0}, size - m_pos);
            memcpy(m_buffer + m_pos, data.data(), (int)write_len);
            m_pos += write_len;
            if (m_pos == size)
                flush();
            data = data.substr(write_len);
        }
    }

    void flush()
    {
        Kakoune::write<atomic>(m_fd, {m_buffer, m_pos});
        m_pos = 0;
    }

private:
    static constexpr ByteCount size = buffer_size;
    int m_fd;
    int m_exception_count;
    ByteCount m_pos = 0;
    char m_buffer[(int)size];
};

}

#endif // file_hh_INCLUDED
