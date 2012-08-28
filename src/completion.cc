#include "completion.hh"

#include "buffer_manager.hh"
#include "utils.hh"

#include <dirent.h>
#include <algorithm>

namespace Kakoune
{

CandidateList complete_filename(const Context& context,
                                const String& prefix,
                                CharCount cursor_pos)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    String dirname = "./";
    String dirprefix;
    String fileprefix = real_prefix;

    CharCount dir_end = -1;
    for (CharCount i = 0; i < real_prefix.length(); ++i)
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

    while (dirent* entry = readdir(dir))
    {
        String filename = entry->d_name;
        if (filename.empty())
            continue;

        if (filename.substr(0, fileprefix.length()) == fileprefix)
        {
            String name = dirprefix + filename;
            if (entry->d_type == DT_DIR)
                name += '/';
            if (fileprefix.length() != 0 or filename[0] != '.')
                result.push_back(name);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

}
