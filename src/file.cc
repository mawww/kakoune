#include "file.hh"

#include "assert.hh"
#include "buffer.hh"
#include "unicode.hh"
#include "ranked_match.hh"
#include "regex.hh"
#include "string.hh"

#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#define st_mtim st_mtimespec
#endif

#if defined(__HAIKU__)
#include <app/Application.h>
#include <app/Roster.h>
#include <storage/Path.h>
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
    auto it = find(path | reverse(), '/');
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
    if (!::getcwd(cwd, 1024))
        throw runtime_error(format("unable to get the current working directory (errno: {})", ::strerror(errno)));

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

MappedFile::MappedFile(StringView filename)
{
    String real_filename = real_path(parse_filename(filename));

    fd = open(real_filename.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd == -1)
    {
        if (errno == ENOENT)
            throw file_not_found{real_filename};

        throw file_access_error(real_filename, strerror(errno));
    }

    fstat(fd, &st);
    if (S_ISDIR(st.st_mode))
        throw file_access_error(real_filename, "is a directory");

    data = (const char*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
}

MappedFile::~MappedFile()
{
    if (fd != -1)
    {
        munmap((void*)data, st.st_size);
        close(fd);
    }
}

bool file_exists(StringView filename)
{
    String real_filename = real_path(parse_filename(filename));
    struct stat st;
    return stat(real_filename.c_str(), &st) == 0;
}

void write(int fd, StringView data)
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
    auto eolformat = buffer.options()["eolformat"].get<EolFormat>();
    StringView eoldata;
    if (eolformat == EolFormat::Crlf)
        eoldata = "\r\n";
    else
        eoldata = "\n";

    if (buffer.options()["BOM"].get<ByteOrderMark>() == ByteOrderMark::Utf8)
        if (::write(fd, "\xEF\xBB\xBF", 3) < 0)
            throw runtime_error(format("unable to write data to the buffer (fd: {}; errno: {})", fd, ::strerror(errno)));

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
        else
        {
            auto old_mask = umask(0);
            auto restore_mask = on_scope_end([old_mask]() { umask(old_mask); });

            if (mkdir(dirname.zstr(), S_IRWXU | S_IRWXG | S_IRWXO) != 0)
                throw runtime_error(format("mkdir failed for directory '{}' errno {}", dirname, errno));
        }
    }
}

template<typename Filter>
Vector<String> list_files(StringView dirname, Filter filter)
{
    char buffer[PATH_MAX+1];
    format_to(buffer, "{}", dirname);
    DIR* dir = opendir(dirname.empty() ? "./" : buffer);
    if (not dir)
        return {};

    auto closeDir = on_scope_end([=]{ closedir(dir); });

    Vector<String> result;
    while (dirent* entry = readdir(dir))
    {
        StringView filename = entry->d_name;
        if (filename.empty() or not filter(*entry))
            continue;

        struct stat st;
        auto fmt_str = (dirname.empty() or dirname.back() == '/') ? "{}{}" : "{}/{}";
        format_to(buffer, fmt_str, dirname, filename);
        if (stat(buffer, &st) != 0)
            continue;

        if (S_ISDIR(st.st_mode))
            filename = format_to(buffer, "{}/", filename);
        result.push_back(filename.str());
    }
    return result;
}

Vector<String> list_files(StringView directory)
{
    return list_files(directory, [](const dirent& entry) {
                          return StringView{entry.d_name}.substr(0_byte, 1_byte) != ".";
                      });
}

static CandidateList candidates(ConstArrayView<RankedMatch> matches, StringView dirname)
{
    CandidateList res;
    res.reserve(matches.size());
    for (auto& match : matches)
        res.push_back(dirname + match.candidate());
    return res;
}

CandidateList complete_filename(StringView prefix,
                                const Regex& ignored_regex,
                                ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    StringView dirname, fileprefix;
    std::tie(dirname, fileprefix) = split_path(real_prefix);

    const bool check_ignored_regex = not ignored_regex.empty() and
        not Kakoune::regex_match(fileprefix.begin(), fileprefix.end(), ignored_regex);
    const bool include_hidden = fileprefix.substr(0_byte, 1_byte) == ".";

    auto filter = [&ignored_regex, check_ignored_regex, include_hidden](const dirent& entry)
    {
        return (include_hidden or StringView{entry.d_name}.substr(0_byte, 1_byte) != ".") and
               (not check_ignored_regex or not Kakoune::regex_match(entry.d_name, ignored_regex));
    };
    auto files = list_files(dirname, filter);
    Vector<RankedMatch> matches;
    for (auto& file : files)
    {
        if (RankedMatch match{file, fileprefix})
            matches.push_back(match);
    }
    std::sort(matches.begin(), matches.end());
    return candidates(matches, dirname);
}

Vector<String> complete_command(StringView prefix, ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    StringView dirname, fileprefix;
    std::tie(dirname, fileprefix) = split_path(real_prefix);

    if (not dirname.empty())
    {
        auto filter = [&dirname](const dirent& entry)
        {
            char buffer[PATH_MAX+1];
            format_to(buffer, "{}{}", dirname, entry.d_name);
            struct stat st;
            if (stat(buffer, &st) != 0)
                return false;
            bool executable = (st.st_mode & S_IXUSR)
                            | (st.st_mode & S_IXGRP)
                            | (st.st_mode & S_IXOTH);
            return S_ISDIR(st.st_mode) or (S_ISREG(st.st_mode) and executable);
        };
        auto files = list_files(dirname, filter);
        Vector<RankedMatch> matches;
        for (auto& file : files)
        {
            if (RankedMatch match{file, real_prefix})
                matches.push_back(match);
        }
        std::sort(matches.begin(), matches.end());
        return candidates(matches, dirname);
    }

    typedef decltype(stat::st_mtim) TimeSpec;

    struct CommandCache
    {
        TimeSpec mtim = {};
        Vector<String> commands;
    };
    static UnorderedMap<String, CommandCache, MemoryDomain::Commands> command_cache;

    Vector<RankedMatch> matches;
    for (auto dir : StringView{getenv("PATH")} | split<StringView>(':'))
    {
        auto dirname = ((not dir.empty() and dir.back() == '/') ? dir.substr(0, dir.length()-1) : dir).str();

        struct stat st;
        if (stat(dirname.c_str(), &st))
            continue;

        auto& cache = command_cache[dirname];
        if (memcmp(&cache.mtim, &st.st_mtim, sizeof(TimeSpec)) != 0)
        {
            auto filter = [&dirname](const dirent& entry) {
                struct stat st;
                char buffer[PATH_MAX+1];
                format_to(buffer, "{}/{}", dirname, entry.d_name);
                if (stat(buffer, &st))
                    return false;
                bool executable = (st.st_mode & S_IXUSR)
                                | (st.st_mode & S_IXGRP)
                                | (st.st_mode & S_IXOTH);
                return S_ISREG(st.st_mode) and executable;
            };

            cache.commands = list_files(dirname, filter);
            memcpy(&cache.mtim, &st.st_mtim, sizeof(TimeSpec));
        }
        for (auto& cmd : cache.commands)
        {
            if (RankedMatch match{cmd, fileprefix})
                matches.push_back(match);
        }
    }
    std::sort(matches.begin(), matches.end());
    auto it = std::unique(matches.begin(), matches.end());
    matches.erase(it, matches.end());
    return candidates(matches, "");
}

timespec get_fs_timestamp(StringView filename)
{
    struct stat st;
    if (stat(filename.zstr(), &st) != 0)
        return InvalidTime;
    return st.st_mtim;
}

String get_kak_binary_path()
{
    char buffer[2048];
#if defined(__linux__) or defined(__CYGWIN__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048);
    kak_assert(res != -1);
    buffer[res] = '\0';
    return buffer;
#elif defined(__FreeBSD__)
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    size_t res = sizeof(buffer);
    sysctl(mib, 4, buffer, &res, NULL, 0);
    return buffer;
#elif defined(__APPLE__)
    uint32_t bufsize = 2048;
    _NSGetExecutablePath(buffer, &bufsize);
    char* canonical_path = realpath(buffer, nullptr);
    String path = canonical_path;
    free(canonical_path);
    return path;
#elif defined(__HAIKU__)
    BApplication app("application/x-vnd.kakoune");
    app_info info;
    status_t status = app.GetAppInfo(&info);
    kak_assert(status == B_OK);
    BPath path(&info.ref);
    return path.Path();
#elif defined(__DragonFly__)
    ssize_t res = readlink("/proc/curproc/file", buffer, 2048);
    kak_assert(res != -1);
    buffer[res] = '\0';
    return buffer;
#else
# error "finding executable path is not implemented on this platform"
#endif
}

}
