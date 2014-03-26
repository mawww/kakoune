#include "file.hh"

#include "assert.hh"
#include "buffer.hh"
#include "buffer_manager.hh"
#include "completion.hh"
#include "debug.hh"
#include "unicode.hh"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

namespace Kakoune
{

String parse_filename(const String& filename)
{
    if (filename.length() >= 1 and filename[0] == '~' and
        (filename.length() == 1 or filename[1] == '/'))
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
            String var_name = filename.substr(i+1, end - i - 1);
            const char* var_value = getenv(var_name.c_str());
            if (var_value)
                result += var_value;

            pos = end;
        }
    }
    if (pos != filename.length())
        result += filename.substr(pos);

    return result;
}

String real_path(const String& filename)
{
    String dirname = ".";
    String basename = filename;

    auto it = find(reversed(filename), '/');
    if (it != filename.rend())
    {
        dirname = String{filename.begin(), it.base()};
        basename = String{it.base(), filename.end()};
    }

    char buffer[PATH_MAX+1];
    char* res = realpath(dirname.c_str(), buffer);
    if (not res)
        throw file_not_found{dirname};
    return res + "/"_str + basename;
}

String compact_path(const String& filename)
{
    String real_filename = real_path(filename);

    char cwd[1024];
    getcwd(cwd, 1024);
    String real_cwd = real_path(cwd) + '/';
    if (prefix_match(real_filename, real_cwd))
        return real_filename.substr(real_cwd.length());

    const char* home = getenv("HOME");
    if (home)
    {
        ByteCount home_len = (int)strlen(home);
        if (real_filename.substr(0, home_len) == home)
            return "~" + real_filename.substr(home_len);
    }

    return filename;
}

String read_file(const String& filename)
{
    int fd = open(parse_filename(filename).c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            throw file_not_found(filename);

        throw file_access_error(filename, strerror(errno));
    }
    auto close_fd = on_scope_end([fd]{ close(fd); });

    String content;
    char buf[256];
    while (true)
    {
        ssize_t size = read(fd, buf, 256);
        if (size == -1 or size == 0)
            break;

        content += String(buf, buf + size);
    }
    return content;
}

Buffer* create_buffer_from_file(String filename)
{
    filename = real_path(parse_filename(filename));

    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        if (errno == ENOENT)
            return nullptr;

        throw file_access_error(filename, strerror(errno));
    }
    struct stat st;
    fstat(fd, &st);
    if (S_ISDIR(st.st_mode))
    {
        close(fd);
        throw file_access_error(filename, "is a directory");
    }
    const char* data = (const char*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    auto cleanup = on_scope_end([&]{ munmap((void*)data, st.st_size); close(fd); });

    const char* pos = data;
    bool crlf = false;
    bool bom  = false;
    if (st.st_size >= 3 and
       data[0] == '\xEF' and data[1] == '\xBB' and data[2] == '\xBF')
    {
        bom = true;
        pos = data + 3;
    }

    std::vector<String> lines;
    const char* end = data + st.st_size;
    while (pos < end)
    {
        const char* line_end = pos;
        while (line_end < end and *line_end != '\r' and *line_end != '\n')
             ++line_end;

        // this should happen only when opening a file which has no
        // end of line as last character.
        if (line_end == end)
        {
            lines.emplace_back(pos, line_end);
            lines.back() += '\n';
            break;
        }

        lines.emplace_back(pos, line_end + 1);
        lines.back().back() = '\n';

        if (line_end+1 != end and *line_end == '\r' and *(line_end+1) == '\n')
        {
            crlf = true;
            pos = line_end + 2;
        }
        else
            pos = line_end + 1;
    }
    Buffer* buffer = BufferManager::instance().get_buffer_ifp(filename);
    if (buffer)
        buffer->reload(std::move(lines), st.st_mtime);
    else
        buffer = new Buffer{filename, Buffer::Flags::File,
                            std::move(lines), st.st_mtime};

    OptionManager& options = buffer->options();
    options.get_local_option("eolformat").set<String>(crlf ? "crlf" : "lf");
    options.get_local_option("BOM").set<String>(bom ? "utf-8" : "no");

    return buffer;
}

static void write(int fd, memoryview<char> data, const String& filename)
{
    const char* ptr = data.pointer();
    ssize_t count   = data.size();

    while (count)
    {
        ssize_t written = ::write(fd, ptr, count);
        ptr += written;
        count -= written;

        if (written == -1)
            throw file_access_error(filename, strerror(errno));
    }
}

void write_buffer_to_file(Buffer& buffer, const String& filename)
{
    buffer.run_hook_in_own_context("BufWritePre", buffer.name());

    String eolformat = buffer.options()["eolformat"].get<String>();
    if (eolformat == "crlf")
        eolformat = "\r\n";
    else
        eolformat = "\n";
    auto eoldata = eolformat.data();

    {
        int fd = open(parse_filename(filename).c_str(),
                      O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd == -1)
            throw file_access_error(filename, strerror(errno));
        auto close_fd = on_scope_end([fd]{ close(fd); });

        if (buffer.options()["BOM"].get<String>() == "utf-8")
            ::write(fd, "\xEF\xBB\xBF", 3);

        for (LineCount i = 0; i < buffer.line_count(); ++i)
        {
            // end of lines are written according to eolformat but always
            // stored as \n
            memoryview<char> linedata = buffer[i].data();
            write(fd, linedata.subrange(0, linedata.size()-1), filename);
            write(fd, eoldata, filename);
        }
    }
    if ((buffer.flags() & Buffer::Flags::File) and filename == buffer.name())
        buffer.notify_saved();

    buffer.run_hook_in_own_context("BufWritePost", buffer.name());
}

String find_file(const String& filename, memoryview<String> paths)
{
    struct stat buf;
    if (filename.size() > 1 and filename[0] == '/')
    {
        if (stat(filename.c_str(), &buf) == 0 and S_ISREG(buf.st_mode))
            return filename;
         return "";
    }
    if (filename.size() > 2 and
             filename[0] == '~' and filename[1] == '/')
    {
        String candidate = getenv("HOME") + filename.substr(1_byte);
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

template<typename Filter>
std::vector<String> list_files(const String& prefix,
                               const String& dirname,
                               Filter filter)
{
    kak_assert(dirname.empty() or dirname.back() == '/');
    DIR* dir = opendir(dirname.empty() ? "./" : dirname.c_str());
    auto closeDir = on_scope_end([=]{ closedir(dir); });

    std::vector<String> result;
    if (not dir)
        return result;

    std::vector<String> subseq_result;
    while (dirent* entry = readdir(dir))
    {
        if (not filter(*entry))
            continue;

        String filename = entry->d_name;
        if (filename.empty())
            continue;

        const bool match_prefix = prefix_match(filename, prefix);
        const bool match_subseq = subsequence_match(filename, prefix);
        if (match_prefix or match_subseq)
        {
            if (entry->d_type == DT_DIR)
                filename += '/';
            if (prefix.length() != 0 or filename[0] != '.')
            {
                if (match_prefix)
                    result.push_back(filename);
                if (match_subseq)
                    subseq_result.push_back(filename);
            }
        }
    }
    auto& real_result = result.empty() ? subseq_result : result;
    return real_result;
}

std::vector<String> complete_filename(const String& prefix,
                                      const Regex& ignored_regex,
                                      ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    String dirname;
    String fileprefix = real_prefix;

    ByteCount dir_end = -1;
    for (ByteCount i = 0; i < real_prefix.length(); ++i)
    {
        if (real_prefix[i] == '/')
            dir_end = i;
    }
    if (dir_end != -1)
    {
        dirname = real_prefix.substr(0, dir_end + 1);
        fileprefix = real_prefix.substr(dir_end + 1);
    }

    const bool check_ignored_regex = not ignored_regex.empty() and
        not boost::regex_match(fileprefix.c_str(), ignored_regex);

    auto filter = [&](const dirent& entry)
    {
        return not check_ignored_regex or
               not boost::regex_match(entry.d_name, ignored_regex);
    };
    std::vector<String> res = list_files(fileprefix, dirname, filter);
    for (auto& file : res)
        file = escape(dirname + file);
    std::sort(res.begin(), res.end());
    return res;
}

std::vector<String> complete_command(const String& prefix, ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    String dirname;
    String fileprefix = real_prefix;

    ByteCount dir_end = -1;
    for (ByteCount i = 0; i < real_prefix.length(); ++i)
    {
        if (real_prefix[i] == '/')
            dir_end = i;
    }

    std::vector<String> path;
    if (dir_end != -1)
    {
        path.emplace_back(real_prefix.substr(0, dir_end + 1));
        fileprefix = real_prefix.substr(dir_end + 1);
    }
    else
        path = split(getenv("PATH"), ':');

    std::vector<String> res;
    for (auto dirname : path)
    {
        if (not dirname.empty() and dirname.back() != '/')
            dirname += '/';

        auto filter = [&](const dirent& entry) {
            struct stat st;
            if (stat((dirname + entry.d_name).c_str(), &st))
                return false;
            bool executable = (st.st_mode & S_IXUSR)
                            | (st.st_mode & S_IXGRP)
                            | (st.st_mode & S_IXOTH);
            return S_ISREG(st.st_mode) and executable;
        };
        auto completion = list_files(prefix, dirname, filter);
        std::move(completion.begin(), completion.end(), std::back_inserter(res));
    }
    std::sort(res.begin(), res.end());
    auto it = std::unique(res.begin(), res.end());
    res.erase(it, res.end());
    return res;
}

time_t get_fs_timestamp(const String& filename)
{
    struct stat st;
    if (stat(filename.c_str(), &st) != 0)
        return InvalidTime;
    return st.st_mtime;
}

}
