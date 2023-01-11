#include "file.hh"

#include "assert.hh"
#include "buffer.hh"
#include "exception.hh"
#include "flags.hh"
#include "option_types.hh"
#include "event_manager.hh"
#include "ranked_match.hh"
#include "regex.hh"
#include "string.hh"
#include "unicode.hh"

#include <limits>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#if defined(__FreeBSD__) or defined(__NetBSD__)
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

struct file_access_error : runtime_error
{
public:
    file_access_error(StringView filename,
                      StringView error_desc)
        : runtime_error(format("{}: {}", filename, error_desc)) {}

    file_access_error(int fd, StringView error_desc)
        : runtime_error(format("fd {}: {}", fd, error_desc)) {}
};

String parse_filename(StringView filename, StringView buf_dir)
{
    auto prefix = filename.substr(0_byte, 2_byte);
    if (prefix == "~" or prefix == "~/")
        return homedir() + filename.substr(1_byte);
    if ((prefix == "%" or prefix == "%/") and not buf_dir.empty())
        return buf_dir + filename.substr(1_byte);
    return filename.str();
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
    if (filename.empty())
        return {};

    char buffer[PATH_MAX+1];

    StringView existing = filename;
    StringView non_existing{};

    while (true)
    {
        if (char* res = realpath(existing.zstr(), buffer))
        {
            if (non_existing.empty())
                return res;

            StringView dir = res;
            while (not dir.empty() and dir.back() == '/')
                dir = dir.substr(0_byte, dir.length()-1_byte);
            return format("{}/{}", dir, non_existing);
        }

        auto it = find(existing.rbegin() + 1, existing.rend(), '/');
        if (it == existing.rend())
        {
            char cwd[1024];
            return format("{}/{}", getcwd(cwd, 1024), filename);
        }

        existing = StringView{existing.begin(), it.base()};
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

    StringView home = homedir();
    while (not home.empty() and home.back() == '/')
        home = home.substr(0_byte, home.length()-1_byte);

    if (not home.empty())
    {
        ByteCount home_len = home.length();
        if (real_filename.substr(0, home_len) == home)
            return "~" + real_filename.substr(home_len);
    }

    return filename.str();
}

StringView tmpdir()
{
    StringView tmpdir = getenv("TMPDIR");
    if (not tmpdir.empty())
        return tmpdir.back() == '/' ? tmpdir.substr(0_byte, tmpdir.length()-1)
                                    : tmpdir;
    return "/tmp";
}

StringView homedir()
{
    StringView home = getenv("HOME");
    if (home.empty())
        return getpwuid(geteuid())->pw_dir;
    return home;
}

bool fd_readable(int fd)
{
    kak_assert(fd >= 0);
    fd_set  rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    timeval tv{0,0};
    return select(fd+1, &rfds, nullptr, nullptr, &tv) == 1;
}

bool fd_writable(int fd)
{
    kak_assert(fd >= 0);
    fd_set  wfds;
    FD_ZERO(&wfds);
    FD_SET(fd, &wfds);

    timeval tv{0,0};
    return select(fd+1, nullptr, &wfds, nullptr, &tv) == 1;
}

String read_fd(int fd, bool text)
{
    String content;
    constexpr size_t bufsize = 256;
    char buf[bufsize];
    while (ssize_t size = read(fd, buf, bufsize))
    {
        if (size == -1)
            throw file_access_error{fd, strerror(errno)};

        if  (text)
        {
            for (StringView data{buf, buf + size}; not data.empty();)
            {
               auto it = find(data, '\r');
               content += StringView{data.begin(), it};
               data = StringView{(it != data.end()) ? it+1 : it, data.end()};
            }
        }
        else
            content += StringView{buf, buf + size};
    }
    return content;
}

String read_file(StringView filename, bool text)
{
    int fd = open(filename.zstr(), O_RDONLY);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));

    auto close_fd = on_scope_end([fd]{ close(fd); });
    return read_fd(fd, text);
}

MappedFile::MappedFile(StringView filename)
    : data{nullptr}
{
    int fd = open(filename.zstr(), O_RDONLY | O_NONBLOCK);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));
    auto close_fd = on_scope_end([&] { close(fd); });

    fstat(fd, &st);
    if (S_ISDIR(st.st_mode))
        throw file_access_error(filename, "is a directory");

    if (st.st_size == 0)
        return;

    data = (const char*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED)
        throw file_access_error{filename, strerror(errno)};
}

MappedFile::~MappedFile()
{
    if (data != nullptr)
        munmap((void*)data, st.st_size);
}

MappedFile::operator StringView() const
{
    if (st.st_size > std::numeric_limits<int>::max())
        throw runtime_error("file is too big");
    return { data, (int)st.st_size };
}

bool file_exists(StringView filename)
{
    struct stat st;
    return stat(filename.zstr(), &st) == 0;
}

bool regular_file_exists(StringView filename)
{
    struct stat st;
    return stat(filename.zstr(), &st) == 0 and
           (st.st_mode & S_IFMT) == S_IFREG;
}

void write(int fd, StringView data)
{
    const char* ptr = data.data();
    ssize_t count   = (int)data.length();

    int flags = fcntl(fd, F_GETFL, 0);
    if (EventManager::has_instance())
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    auto restore_flags = on_scope_end([&] { fcntl(fd, F_SETFL, flags); });

    while (count)
    {
        if (ssize_t written = ::write(fd, ptr, count); written != -1)
        {
            ptr += written;
            count -= written;
        }
        else if (errno == EAGAIN and EventManager::has_instance())
            EventManager::instance().handle_next_events(EventMode::Urgent, nullptr, false);
        else
            throw file_access_error(format("fd: {}", fd), strerror(errno));
    }
}

void write_to_file(StringView filename, StringView data)
{
    const int fd = open(filename.zstr(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));
    auto close_fd = on_scope_end([fd]{ close(fd); });
    write(fd, data);
}

void write_buffer_to_fd(Buffer& buffer, int fd)
{
    auto eolformat = buffer.options()["eolformat"].get<EolFormat>();
    StringView eoldata;
    if (eolformat == EolFormat::Crlf)
        eoldata = "\r\n";
    else
        eoldata = "\n";


    BufferedWriter writer{fd};
    if (buffer.options()["BOM"].get<ByteOrderMark>() == ByteOrderMark::Utf8)
        writer.write("\xEF\xBB\xBF");

    for (LineCount i = 0; i < buffer.line_count(); ++i)
    {
        // end of lines are written according to eolformat but always
        // stored as \n
        StringView linedata = buffer[i];
        writer.write(linedata.substr(0, linedata.length()-1));
        writer.write(eoldata);
    }
}

int open_temp_file(StringView filename, char (&buffer)[PATH_MAX])
{
    String path = real_path(filename);
    auto [dir,file] = split_path(path);

    if (dir.empty())
        format_to(buffer, ".{}.kak.XXXXXX", file);
    else
        format_to(buffer, "{}/.{}.kak.XXXXXX", dir, file);

    return mkstemp(buffer);
}

int open_temp_file(StringView filename)
{
    char buffer[PATH_MAX];
    return open_temp_file(filename, buffer);
}

void write_buffer_to_file(Buffer& buffer, StringView filename,
                          WriteMethod method, WriteFlags flags)
{
    auto zfilename = filename.zstr();
    struct stat st;

    bool replace = method == WriteMethod::Replace;
    bool force = flags & WriteFlags::Force;

    if ((replace or force) and (::stat(zfilename, &st) != 0 or
                                (st.st_mode & S_IFMT) != S_IFREG))
    {
        force = false;
        replace = false;
    }

    if (force and ::chmod(zfilename, st.st_mode | S_IWUSR) < 0)
        throw runtime_error("unable to change file permissions");

    auto restore_mode = on_scope_end([&]{
        if ((force or replace) and ::chmod(zfilename, st.st_mode) < 0)
            throw runtime_error("unable to restore file permissions");
    });

    char temp_filename[PATH_MAX];
    const int fd = replace ? open_temp_file(filename, temp_filename)
                           : open(zfilename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd == -1)
        throw file_access_error(filename, strerror(errno));

    {
        auto close_fd = on_scope_end([fd]{ close(fd); });
        write_buffer_to_fd(buffer, fd);
        if (flags & WriteFlags::Sync)
            ::fsync(fd);
    }

    if (replace and rename(temp_filename, zfilename) != 0)
        throw runtime_error("replacing file failed");

    if ((buffer.flags() & Buffer::Flags::File) and
        real_path(filename) == real_path(buffer.name()))
        buffer.notify_saved(get_fs_status(real_path(filename)));
}

void write_buffer_to_backup_file(Buffer& buffer)
{
    const int fd = open_temp_file(buffer.name());
    if (fd >= 0)
    {
        write_buffer_to_fd(buffer, fd);
        close(fd);
    }
}

String find_file(StringView filename, StringView buf_dir, ConstArrayView<String> paths)
{
    struct stat buf;
    if (filename.substr(0_byte, 1_byte) == "/")
    {
        if (stat(filename.zstr(), &buf) == 0 and S_ISREG(buf.st_mode))
            return filename.str();
         return "";
    }
    if (filename.substr(0_byte, 2_byte) == "~/")
    {
        String candidate = homedir() + filename.substr(1_byte);
        if (stat(candidate.c_str(), &buf) == 0 and S_ISREG(buf.st_mode))
            return candidate;
        return "";
    }

    for (auto candidate : paths | transform([&](StringView s) { return parse_filename(s, buf_dir); }))
    {
        if (not candidate.empty() and candidate.back() != '/')
            candidate += '/';
        candidate += filename;
        if (stat(candidate.c_str(), &buf) == 0 and S_ISREG(buf.st_mode))
            return candidate;
    }
    return "";
}

void make_directory(StringView dir, mode_t mode)
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
                throw runtime_error(format("cannot make directory, '{}' exists but is not a directory", dirname));
        }
        else
        {
            auto old_mask = umask(0);
            auto restore_mask = on_scope_end([old_mask]() { umask(old_mask); });

            if (mkdir(dirname.zstr(), mode) != 0)
                throw runtime_error(format("mkdir failed for directory '{}' errno {}", dirname, errno));
        }
    }
}

template<MemoryDomain domain = MemoryDomain::Undefined, typename Filter>
Vector<String, domain> list_files(StringView dirname, Filter filter)
{
    char buffer[PATH_MAX+1];
    format_to(buffer, "{}", dirname);
    DIR* dir = opendir(dirname.empty() ? "./" : buffer);
    if (not dir)
        return {};

    auto close_dir = on_scope_end([dir]{ closedir(dir); });

    Vector<String, domain> result;
    while (dirent* entry = readdir(dir))
    {
        StringView filename = entry->d_name;
        if (filename.empty())
            continue;

        struct stat st;
        auto fmt_str = (dirname.empty() or dirname.back() == '/') ? "{}{}" : "{}/{}";
        format_to(buffer, fmt_str, dirname, filename);
        if (stat(buffer, &st) != 0 or not filter(*entry, st))
            continue;

        if (S_ISDIR(st.st_mode))
            filename = format_to(buffer, "{}/", filename);
        result.push_back(filename.str());
    }
    return result;
}

template<MemoryDomain domain = MemoryDomain::Undefined>
Vector<String, domain> list_files(StringView directory)
{
    return list_files(directory, [](const dirent& entry, const struct stat&) {
                          return StringView{entry.d_name}.substr(0_byte, 1_byte) != ".";
                      });
}

Vector<String> list_files(StringView directory)
{
    return list_files<>(directory);
}

static CandidateList candidates(ConstArrayView<RankedMatch> matches, StringView dirname)
{
    CandidateList res;
    res.reserve(matches.size());
    for (auto& match : matches)
        res.push_back(dirname + match.candidate());
    return res;
}

CandidateList complete_filename(StringView prefix, const Regex& ignored_regex,
                                ByteCount cursor_pos, FilenameFlags flags)
{
    prefix = prefix.substr(0, cursor_pos);
    auto [dirname, fileprefix] = split_path(prefix);
    auto parsed_dirname = parse_filename(dirname);

    Optional<ThreadedRegexVM<const char*, RegexMode::Forward | RegexMode::AnyMatch | RegexMode::NoSaves>> vm;
    if (not ignored_regex.empty())
    {
        vm.emplace(*ignored_regex.impl());
        if (vm->exec(fileprefix.begin(), fileprefix.end(), fileprefix.begin(), fileprefix.end(), RegexExecFlags::None))
            vm.reset();
    }

    const bool only_dirs = (flags & FilenameFlags::OnlyDirectories);

    auto filter = [&vm, only_dirs](const dirent& entry, struct stat& st)
    {
        StringView name{entry.d_name};
        return (not vm or not vm->exec(name.begin(), name.end(), name.begin(), name.end(), RegexExecFlags::None)) and
               (not only_dirs or S_ISDIR(st.st_mode));
    };
    auto files = list_files(parsed_dirname, filter);
    Vector<RankedMatch> matches;
    for (auto& file : files)
    {
        if (RankedMatch match{file, fileprefix})
            matches.push_back(match);
    }
    std::sort(matches.begin(), matches.end());
    const bool expand = (flags & FilenameFlags::Expand);
    return candidates(matches, expand ? parsed_dirname : dirname);
}

CandidateList complete_command(StringView prefix, ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    auto [dirname, fileprefix] = split_path(real_prefix);

    if (not dirname.empty())
    {
        auto filter = [](const dirent& entry, const struct stat& st)
        {
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

    using TimeSpec = decltype(stat::st_mtim);

    struct CommandCache
    {
        TimeSpec mtim = {};
        Vector<String, MemoryDomain::Completion> commands;
    };
    static HashMap<String, CommandCache, MemoryDomain::Commands> command_cache;

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
            auto filter = [](const dirent& entry, const struct stat& st) {
                bool executable = (st.st_mode & S_IXUSR)
                                | (st.st_mode & S_IXGRP)
                                | (st.st_mode & S_IXOTH);
                return S_ISREG(st.st_mode) and executable;
            };

            cache.commands = list_files<MemoryDomain::Completion>(dirname, filter);
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

FsStatus get_fs_status(StringView filename)
{
    MappedFile fd{filename};

    return {fd.st.st_mtim, fd.st.st_size, hash_data(fd.data, fd.st.st_size)};
}

String get_kak_binary_path()
{
    char buffer[2048];
#if defined(__linux__) or defined(__CYGWIN__) or defined(__gnu_hurd__)
    ssize_t res = readlink("/proc/self/exe", buffer, 2048);
    kak_assert(res != -1);
    buffer[res] = '\0';
    return buffer;
#elif defined(__FreeBSD__) or defined(__NetBSD__)
#if defined(__FreeBSD__)
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
#elif defined(__NetBSD__)
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME};
#endif
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
#elif defined(__OpenBSD__)
    return KAK_BIN_PATH;
#elif defined(__sun__)
    ssize_t res = readlink("/proc/self/path/a.out", buffer, 2048);
    kak_assert(res != -1);
    buffer[res] = '\0';
    return buffer;
#else
# error "finding executable path is not implemented on this platform"
#endif
}

}
