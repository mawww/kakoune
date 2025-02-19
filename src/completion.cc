#include "completion.hh"
#include "file.hh"
#include "context.hh"
#include "option_types.hh"
#include "option_manager.hh"
#include "regex.hh"

namespace Kakoune
{

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

    Vector<String> files;
    list_files(parsed_dirname, [&](StringView filename, const struct stat& st) {
        if ((not vm or not vm->exec(filename.begin(), filename.end(),
                                    filename.begin(), filename.end(),
                                    RegexExecFlags::None)) and
               (not only_dirs or S_ISDIR(st.st_mode)))
            files.push_back(filename.str());
    });
    Vector<RankedMatch> matches;
    for (auto& file : files)
    {
        if (RankedMatch match{file, fileprefix})
            matches.push_back(match);
    }
    // Hack: when completing directories, also echo back the query if it
    // is a valid directory. This enables menu completion to select the
    // directory instead of a child.
    if (only_dirs and not dirname.empty() and dirname.back() == '/' and fileprefix.empty()
        and /* exists on disk */ not files.empty())
    {
        matches.push_back(RankedMatch{fileprefix, fileprefix});
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
        Vector<String> files;
        list_files(dirname, [&](StringView filename, const struct stat& st) {
            bool executable = (st.st_mode & S_IXUSR)
                            | (st.st_mode & S_IXGRP)
                            | (st.st_mode & S_IXOTH);
            if (S_ISDIR(st.st_mode) or (S_ISREG(st.st_mode) and executable))
                files.push_back(filename.str());
        });
        Vector<RankedMatch> matches;
        for (auto& file : files)
        {
            if (RankedMatch match{file, fileprefix})
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
    static HashMap<String, CommandCache, MemoryDomain::Completion> command_cache;

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
            cache.commands.clear();
            list_files(dirname, [&](StringView filename, const struct stat& st) {
                bool executable = (st.st_mode & S_IXUSR)
                                | (st.st_mode & S_IXGRP)
                                | (st.st_mode & S_IXOTH);
                if (S_ISREG(st.st_mode) and executable)
                    cache.commands.push_back(filename.str());
            });
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

Completions shell_complete(const Context& context, StringView prefix, ByteCount cursor_pos)
{
    ByteCount word_start = 0;
    ByteCount word_end = 0;

    bool command = true;
    const ByteCount len = prefix.length();
    for (ByteCount pos = 0; pos < cursor_pos and pos < len;)
    {
        command = (pos == 0 or prefix[pos-1] == ';' or prefix[pos-1] == '|' or
                   (pos > 1 and prefix[pos-1] == '&' and prefix[pos-2] == '&'));
        while (pos != len and is_horizontal_blank(prefix[pos]))
            ++pos;
        word_start = pos;
        while (pos != len and not is_horizontal_blank(prefix[pos]))
            ++pos;
        word_end = pos;
    }
    Completions completions{word_start, word_end};
    if (command)
        completions.candidates = complete_command(prefix.substr(word_start, word_end),
                                                  cursor_pos - word_start);
    else
        completions.candidates = complete_filename(prefix.substr(word_start, word_end),
                                                   context.options()["ignored_files"].get<Regex>(),
                                                   cursor_pos - word_start);
    return completions;
}

}
