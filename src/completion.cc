#include "completion.hh"

#include "utils.hh"

#include <dirent.h>

namespace Kakoune
{

CandidateList complete_filename(const std::string& prefix,
                                size_t cursor_pos)
{
    std::string real_prefix = prefix.substr(0, cursor_pos);
    size_t dir_end = real_prefix.find_last_of('/');
    std::string dirname = "./";
    std::string fileprefix = real_prefix;

    if (dir_end != std::string::npos)
    {
        dirname = real_prefix.substr(0, dir_end + 1);
        fileprefix = real_prefix.substr(dir_end + 1, std::string::npos);
    }

    auto dir = auto_raii(opendir(dirname.c_str()), closedir);

    CandidateList result;
    while (dirent* entry = readdir(dir))
    {
        std::string filename = entry->d_name;
        if (filename.substr(0, fileprefix.length()) == fileprefix)
            result.push_back(filename);
    }
    return result;
}

}
