#include "file.hh"

#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "unicode.hh"
#include "regex.hh"
#include "string.hh"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace Kakoune
{

String parse_filename(StringView filename)
{
    if (filename.length() >= 1 and filename[0_byte] == '~' and
        (filename.length() == 1 or filename[1_byte] == '/'))
        return parse_filename("$HOME" + filename.substr(1_byte));

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
            StringView var_name = filename.substr(i+1, end - i - 1);
            const char* var_value = getenv(var_name.zstr());
            if (var_value)
                result += var_value;

            pos = end;
        }
    }
    if (pos != filename.length())
        result += filename.substr(pos);

    return result;
}

std::pair<StringView, StringView> split_path(StringView path)
{
    auto it = find(reversed(path), '/');
    if (it == path.rend())
        return { {}, path };
    const char* slash = it.base()-1;
    return { {path.begin(), slash+1}, {slash+1, path.end()} };
}

String real_path(StringView filename)
{
    char buffer[PATH_MAX+1];

    StringView existing = filename;
    StringView non_existing;

    while (true)
    {
        char* res = realpath(existing.zstr(), buffer);
        if (res)
        {
            if (non_existing.empty())
                return res;
            return format("{}/{}", res, non_existing);
        }

        auto it = find(existing.rbegin(), existing.rend(), '/');
        if (it == existing.rend())
            return filename.str();

        existing = StringView{existing.begin(), it.base()-1};
        non_existing = StringView{it.base(), filename.end()};
    }
}

String compact_path(StringView filename)
{
    String real_filename = real_path(filename);

    char cwd[1024];
    getcwd(cwd, 1024);
    String real_cwd = real_path(cwd) + "/";
    if (prefix_match(real_filename, real_cwd))
        return real_filename.substr(real_cwd.length()).str();

    const char* home = getenv("HOME");
    if (home)
    {
        ByteCount home_len = (int)strlen(home);
        if (real_filename.substr(0, home_len) == home)
            return "~" + real_filename.substr(home_len);
    }

    return filename.str();
}

String read_fd(int fd, bool text)
{
    String content;
    constexpr size_t bufsize = 256;
    char buf[bufsize];
    while (true)
    {
        ssize_t size = read(fd, buf, bufsize);
        if (size == -1 or size == 0)
            break;

        if  (text)
        {
            ssize_t beg = 0;
            for (ssize_t pos = 0; pos < size; ++pos)
            {
                if (buf[pos] == '\r')
                {
                   content += StringView{buf + beg, buf + pos};
                   beg = pos + 1;
                }
            }
            content += StringView{buf + beg, buf + size};
        }
        else
            content += StringView{buf, buf + size};
    }
    return content;
}

String read_file(StringView filename, bool text)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            throw file_not_found(filename);

        throw file_access_error(filename, strerror(errno));
    }
    auto close_fd = on_scope_end([fd]{ close(fd); });

    return read_fd(fd, text);
}

Buffer* create_buffer_from_file(StringView filename)
{
    String real_filename = real_path(parse_filename(filename));

    int fd = open(real_filename.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1)
    {
        if (errno == ENOENT)
            return nullptr;

        throw file_access_error(real_filename, strerror(errno));
    }
    auto close_fd = on_scope_end([&]{ close(fd); });

    struct stat st;
    fstat(fd, &st);
    if (S_ISDIR(st.st_mode))
        throw file_access_error(real_filename, "is a directory");
    if (S_ISFIFO(st.st_mode)) // Do not try to read fifos, use them as write only
        return create_buffer_from_data({}, real_filename,
                                       Buffer::Flags::File, st.st_mtime);

    const char* data = (const char*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    auto unmap = on_scope_end([&]{ munmap((void*)data, st.st_size); });

    return create_buffer_from_data({data, data + st.st_size}, real_filename,
                                   Buffer::Flags::File, st.st_mtime);
}

static void write(int fd, StringView data)
{
    const char* ptr = data.data();
    ssize_t count   = (int)data.length();

    while (count)
    {
        ssize_t written = ::write(fd, ptr, count);
        ptr += written;
        count -= written;

        if (written == -1)
            throw file_access_error("fd: " + to_string(fd), strerror(errno));
    }
}

void write_buffer_to_fd(Buffer& buffer, int fd)
{
    const String& eolformat = buffer.options()["eolformat"].get<String>();
    StringView eoldata;
    if (eolformat == "crlf")
        eoldata = "\r\n";
    else
        eoldata = "\n";

    if (buffer.options()["BOM"].get<String>() == "utf-8")
        ::write(fd, "\xEF\xBB\xBF", 3);

    for (LineCount i = 0; i < buffer.line_count(); ++i)
    {
        // end of lines are written according to eolformat but always
        // stored as \n
        StringView linedata = buffer[i];
        write(fd, linedata.substr(0, linedata.length()-1));
        write(fd, eoldata);
    }
}

void write_buffer_to_file(Buffer& buffer, StringView filename)
{
    buffer.run_hook_in_own_context("BufWritePre", buffer.name());

    {
        int fd = open(parse_filename(filename).c_str(),
                      O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd == -1)
            throw file_access_error(filename, strerror(errno));
        auto close_fd = on_scope_end([fd]{ close(fd); });

        write_buffer_to_fd(buffer, fd);
    }

    if ((buffer.flags() & Buffer::Flags::File) and
        real_path(filename) == real_path(buffer.name()))
        buffer.notify_saved();

    buffer.run_hook_in_own_context("BufWritePost", buffer.name());
}

void write_buffer_to_backup_file(Buffer& buffer)
{
    String path = real_path(buffer.name());
    StringView dir, file;
    std::tie(dir,file) = split_path(path);

    char pattern[PATH_MAX];
    if (dir.empty())
        format_to(pattern, ".{}.kak.XXXXXX", file);
    else
        format_to(pattern, "{}/.{}.kak.XXXXXX", dir, file);

    int fd = mkstemp(pattern);
    if (fd >= 0)
    {
        write_buffer_to_fd(buffer, fd);
        close(fd);
    }
}

String find_file(StringView filename, ConstArrayView<String> paths)
{
    struct stat buf;
    if (filename.length() > 1 and filename[0_byte] == '/')
    {
        if (stat(filename.zstr(), &buf) == 0 and S_ISREG(buf.st_mode))
            return filename.str();
         return "";
    }
    if (filename.length() > 2 and
             filename[0_byte] == '~' and filename[1_byte] == '/')
    {
        String candidate = getenv("HOME") + filename.substr(1_byte).str();
        if (stat(candidate.c_str(), &buf) == 0 and S_ISREG(buf.st_mode))
            return candidate;
        return "";
    }

    for (auto candidate : paths)
    {
        if (not candidate.empty() and candidate.back() != '/')
            candidate += '/';
        candidate += filename;
        if (stat(candidate.c_str(), &buf) == 0 and S_ISREG(buf.st_mode))
            return candidate;
    }
    return "";
}

void make_directory(StringView dir)
{
    auto it = dir.begin(), end = dir.end();
    while(it != end)
    {
        it = std::find(it+1, end, '/');
        struct stat st;
        StringView dirname{dir.begin(), it};
        if (stat(dirname.zstr(), &st) == 0)
        {
            if (not S_ISDIR(st.st_mode))
                throw runtime_error(format("Cannot make directory, '{}' exists but is not a directory", dirname));
        }
        else if (mkdir(dirname.zstr(), S_IRWXU) != 0)
            throw runtime_error(format("mkdir failed for directory '{}' errno {}", dirname, errno));
    }
}

template<typename Filter>
Vector<String> list_files(StringView prefix, StringView dirname,
                          Filter filter)
{
    kak_assert(dirname.empty() or dirname.back() == '/');
    DIR* dir = opendir(dirname.empty() ? "./" : dirname.zstr());
    if (not dir)
        return {};

    auto closeDir = on_scope_end([=]{ closedir(dir); });

    Vector<String> result;
    Vector<String> subseq_result;
    while (dirent* entry = readdir(dir))
    {
        if (not filter(*entry))
            continue;

        String filename = entry->d_name;
        if (filename.empty())
            continue;

        const bool match_prefix = prefix_match(filename, prefix);
        const bool match_subseq = subsequence_match(filename, prefix);
        struct stat st;
        if ((match_prefix or match_subseq) and
            stat((dirname + filename).c_str(), &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
                filename += '/';
            if (prefix.length() != 0 or filename[0_byte] != '.')
            {
                if (match_prefix)
                    result.push_back(filename);
                if (match_subseq)
                    subseq_result.push_back(filename);
            }
        }
    }
    return result.empty() ? subseq_result : result;
}

Vector<String> list_files(StringView directory)
{
    return list_files("", directory, [](const dirent&) { return true; });
}

CandidateList complete_filename(StringView prefix,
                                const Regex& ignored_regex,
                                ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    StringView dirname, fileprefix;
    std::tie(dirname, fileprefix) = split_path(real_prefix);

    const bool check_ignored_regex = not ignored_regex.empty() and
        not regex_match(fileprefix.begin(), fileprefix.end(), ignored_regex);

    auto filter = [&](const dirent& entry)
    {
        return not check_ignored_regex or
               not regex_match(entry.d_name, ignored_regex);
    };
    Vector<String> res = list_files(fileprefix, dirname, filter);
    for (auto& file : res)
        file = dirname + file;
    std::sort(res.begin(), res.end());
    return res;
}

Vector<String> complete_command(StringView prefix, ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    StringView dirname, fileprefix;
    std::tie(dirname, fileprefix) = split_path(real_prefix);

    if (not dirname.empty())
    {
        auto filter = [&](const dirent& entry)
        {
            struct stat st;
            if (stat((dirname.str() + entry.d_name).c_str(), &st))
                return false;
            bool executable = (st.st_mode & S_IXUSR)
                            | (st.st_mode & S_IXGRP)
                            | (st.st_mode & S_IXOTH);
            return S_ISDIR(st.st_mode) or (S_ISREG(st.st_mode) and executable);
        };
        Vector<String> res = list_files(fileprefix, dirname, filter);
        for (auto& file : res)
            file = dirname + file;
        std::sort(res.begin(), res.end());
        return res;
    }

    typedef decltype(stat::st_mtime) TimeSpec;

    struct CommandCache
    {
        TimeSpec mtime = {};
        Vector<String> commands;
    };
    static UnorderedMap<String, CommandCache> command_cache;

    Vector<String> res;
    for (auto dir : split(getenv("PATH"), ':'))
    {
        String dirname = dir.str();
        if (not dirname.empty() and dirname.back() != '/')
            dirname += '/';

        struct stat st;
        if (stat(dirname.substr(0_byte, dirname.length() - 1).zstr(), &st))
            continue;

        auto& cache = command_cache[dirname];
        if (memcmp(&cache.mtime, &st.st_mtime, sizeof(TimeSpec)) != 0)
        {
            auto filter = [&](const dirent& entry) {
                struct stat st;
                if (stat((dirname + entry.d_name).c_str(), &st))
                    return false;
                bool executable = (st.st_mode & S_IXUSR)
                                | (st.st_mode & S_IXGRP)
                                | (st.st_mode & S_IXOTH);
                return S_ISREG(st.st_mode) and executable;
            };

            cache.commands = list_files("", dirname, filter);
            memcpy(&cache.mtime, &st.st_mtime, sizeof(TimeSpec));
        }
        for (auto& cmd : cache.commands)
        {
            if (prefix_match(cmd, fileprefix))
                res.push_back(cmd);
        }
    }
    std::sort(res.begin(), res.end());
    auto it = std::unique(res.begin(), res.end());
    res.erase(it, res.end());
    return res;
}

time_t get_fs_timestamp(StringView filename)
{
    struct stat st;
    if (stat(filename.zstr(), &st) != 0)
        return InvalidTime;
    return st.st_mtime;
}

String get_kak_binary_path()
{
    char buffer[2048];
#if defined(__linux__) or defined(__CYGWIN__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048);
    kak_assert(res != -1);
    buffer[res] = '\0';
    return buffer;
#elif defined(__APPLE__)
    uint32_t bufsize = 2048;
    _NSGetExecutablePath(buffer, &bufsize);
    char* canonical_path = realpath(buffer, nullptr);
    String path = canonical_path;
    free(canonical_path);
    return path;
#else
# error "finding executable path is not implemented on this platform"
#endif
}

}
