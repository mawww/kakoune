#include "completion.hh"

#include "buffer_manager.hh"
#include "utils.hh"

#include <dirent.h>
#include <algorithm>

namespace Kakoune
{

CandidateList complete_filename(const std::string& prefix,
                                size_t cursor_pos)
{
    std::string real_prefix = prefix.substr(0, cursor_pos);
    size_t dir_end = real_prefix.find_last_of('/');
    std::string dirname = "./";
    std::string dirprefix;
    std::string fileprefix = real_prefix;

    if (dir_end != std::string::npos)
    {
        dirname = real_prefix.substr(0, dir_end + 1);
        dirprefix = dirname;
        fileprefix = real_prefix.substr(dir_end + 1, std::string::npos);
    }

    auto dir = auto_raii(opendir(dirname.c_str()), closedir);

    CandidateList result;
    while (dirent* entry = readdir(dir))
    {
        std::string filename = entry->d_name;
        if (filename.empty())
            continue;

        if (filename.substr(0, fileprefix.length()) == fileprefix)
        {
            std::string name = dirprefix + filename;
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
