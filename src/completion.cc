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

static boost::regex make_regex_ifp(const String& ex)
{
    boost::regex result;
    if (not ex.empty())
    {
        try
        {
            result = boost::regex(ex.c_str());
        }
        catch(boost::regex_error&) {}
    }
    return result;
}

CandidateList complete_filename(const Context& context,
                                const String& prefix,
                                ByteCount cursor_pos)
{
    String real_prefix = parse_filename(prefix.substr(0, cursor_pos));
    String dirname = "./";
    String dirprefix;
    String fileprefix = real_prefix;

    boost::regex ignored_files = make_regex_ifp(context.options()["ignored_files"].as_string());

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
    auto closeDir = on_scope_end([=]{ closedir(dir); });

    CandidateList result;
    if (not dir)
        return result;

    const bool check_ignored_files = not ignored_files.empty() and
        not boost::regex_match(fileprefix.c_str(), ignored_files);

    boost::regex file_regex = make_regex_ifp(fileprefix);
    CandidateList regex_result;
    while (dirent* entry = readdir(dir))
    {
        String filename = entry->d_name;
        if (filename.empty())
            continue;

        if (check_ignored_files and boost::regex_match(filename.c_str(), ignored_files))
            continue;

        const bool match_prefix = (filename.substr(0, fileprefix.length()) == fileprefix);
        const bool match_regex  = not file_regex.empty() and
            boost::regex_match(filename.c_str(), file_regex);

        if (match_prefix or match_regex)
        {
            String name = dirprefix + filename;
            if (entry->d_type == DT_DIR)
                name += '/';
            if (fileprefix.length() != 0 or filename[0] != '.')
            {
                if (match_prefix)
                    result.push_back(escape(name));
                if (match_regex)
                    regex_result.push_back(escape(name));
            }
        }
    }
    CandidateList& real_result = result.empty() ? regex_result : result;
    std::sort(real_result.begin(), real_result.end());
    return real_result;
}

}
