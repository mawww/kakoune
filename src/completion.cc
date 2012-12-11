#include "completion.hh"

#include "buffer_manager.hh"
#include "utils.hh"
#include "file.hh"
#include "context.hh"

#include <dirent.h>
#include <algorithm>

#include <boost/regex.hpp>

namespace Kakoune
{

CandidateList complete_filename(const Context& context,
                                const String& prefix,
                                ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    String dirname = "./";
    String dirprefix;
    String fileprefix = real_prefix;

    String ignored_files = context.options()["ignored_files"].as_string();
    boost::regex ignored_files_regex;
    if (not ignored_files.empty())
         ignored_files_regex = boost::regex(ignored_files.c_str());

    ByteCount dir_end = -1;
    for (ByteCount i = 0; i < real_prefix.length(); ++i)
    {
        if (real_prefix[i] == '/')
            dir_end = i;
    }
    if (dir_end != -1)
    {
        dirname = real_prefix.substr(0, dir_end + 1);
        dirprefix = dirname;
        fileprefix = real_prefix.substr(dir_end + 1);
    }

    DIR* dir = opendir(dirname.c_str());
    auto closeDir = on_scope_end([=](){ closedir(dir); });

    CandidateList result;
    if (not dir)
        return result;

    const bool check_ignored_files = not ignored_files.empty() and
        not boost::regex_match(fileprefix.c_str(), ignored_files_regex);

    while (dirent* entry = readdir(dir))
    {
        String filename = entry->d_name;
        if (filename.empty())
            continue;

        if (check_ignored_files and
            boost::regex_match(filename.c_str(), ignored_files_regex))
            continue;

        if (filename.substr(0, fileprefix.length()) == fileprefix)
        {
            String name = dirprefix + filename;
            if (entry->d_type == DT_DIR)
                name += '/';
            if (fileprefix.length() != 0 or filename[0] != '.')
                result.push_back(escape(name));
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

}
