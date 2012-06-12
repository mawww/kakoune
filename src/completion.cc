#include "completion.hh"

#include "buffer_manager.hh"
#include "utils.hh"

#include <dirent.h>
#include <algorithm>

namespace Kakoune
{

CandidateList complete_filename(const String& prefix,
                                size_t cursor_pos)
{
    String real_prefix = prefix.substr(0, cursor_pos);
    auto dir_end = std::find(real_prefix.begin(), real_prefix.end(), '/');
    String dirname = "./";
    String dirprefix;
    String fileprefix = real_prefix;

    if (dir_end != real_prefix.end())
    {
        dirname = String(real_prefix.begin(), dir_end + 1);
        dirprefix = dirname;
        fileprefix = String(dir_end + 1, real_prefix.end());
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
            if (fileprefix.length() or filename[0] != '.')
                result.push_back(name);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

}
