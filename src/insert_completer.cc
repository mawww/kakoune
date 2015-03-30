#include "insert_completer.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "context.hh"
#include "debug.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
#include "file.hh"
#include "regex.hh"
#include "user_interface.hh"
#include "window.hh"
#include "word_db.hh"

namespace Kakoune
{

using StringList = Vector<String, MemoryDomain::Options>;

String option_to_string(const InsertCompleterDesc& opt)
{
    switch (opt.mode)
    {
        case InsertCompleterDesc::Word:
            return "word=" + (opt.param ? *opt.param : "");
        case InsertCompleterDesc::Filename:
            return "filename";
        case InsertCompleterDesc::Option:
            return "option=" + (opt.param ? *opt.param : "");
    }
    kak_assert(false);
    return "";
}

void option_from_string(StringView str, InsertCompleterDesc& opt)
{
    if (str.substr(0_byte, 7_byte) == "option=")
    {
        opt.mode = InsertCompleterDesc::Option;
        opt.param = str.substr(7_byte).str();
        return;
    }
    else if (str.substr(0_byte, 5_byte) == "word=")
    {
        auto param = str.substr(5_byte);
        if (param == "all" or param == "buffer")
        {
            opt.mode = InsertCompleterDesc::Word;
            opt.param = param.str();
            return;
        }
    }
    else if (str == "filename")
    {
        opt.mode = InsertCompleterDesc::Filename;
        opt.param = Optional<String>{};
        return;
    }
    throw runtime_error("invalid completer description: " + str);;
}

namespace
{

WordDB& get_word_db(const Buffer& buffer)
{
    static const ValueId word_db_id = ValueId::get_free_id();
    Value& cache_val = buffer.values()[word_db_id];
    if (not cache_val)
        cache_val = Value(WordDB{buffer});
    return cache_val.as<WordDB>();
}

template<bool other_buffers, bool subseq>
InsertCompletion complete_word(const Buffer& buffer, ByteCoord cursor_pos)
{
   auto pos = buffer.iterator_at(cursor_pos);
   if (pos == buffer.begin() or not is_word(*utf8::previous(pos, buffer.begin())))
       return {};

    auto end = buffer.iterator_at(cursor_pos);
    auto begin = end-1;
    while (begin != buffer.begin() and is_word(*begin))
        --begin;
    if (not is_word(*begin))
        ++begin;

    String prefix{begin, end};

    while (end != buffer.end() and is_word(*end))
        ++end;

    String current_word{begin, end};

    auto& word_db = get_word_db(buffer);
    auto matches = word_db.find_matching(
        prefix, subseq ? subsequence_match : prefix_match);

    if (word_db.get_word_occurences(current_word) <= 1)
        unordered_erase(matches, current_word);

    if (other_buffers)
    {
        for (const auto& buf : BufferManager::instance())
        {
            if (buf.get() == &buffer)
                continue;
            auto& buf_word_db = get_word_db(*buf);
            auto bufmatches = buf_word_db.find_matching(
                prefix, subseq ? subsequence_match : prefix_match);
            std::move(bufmatches.begin(), bufmatches.end(),
                      std::back_inserter(matches));
        }
    }
    unordered_erase(matches, prefix);
    std::sort(matches.begin(), matches.end());
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

    ComplAndDescList result;
    result.reserve(matches.size());
    for (auto& m : matches)
        result.emplace_back(m.str(), "");

    return { begin.coord(), cursor_pos, std::move(result), buffer.timestamp() };
}

template<bool require_slash>
InsertCompletion complete_filename(const Buffer& buffer, ByteCoord cursor_pos,
                                   OptionManager& options)
{
    auto pos = buffer.iterator_at(cursor_pos);
    auto begin = pos;

    auto is_filename = [](char c)
    {
        return isalnum(c) or c == '/' or c == '.' or c == '_' or c == '-';
    };
    while (begin != buffer.begin() and is_filename(*(begin-1)))
        --begin;

    if (begin == pos)
        return {};

    String prefix{begin, pos};
    if (require_slash and not contains(prefix, '/'))
        return {};

    ComplAndDescList res;
    if (prefix.front() == '/')
    {
        for (auto& filename : Kakoune::complete_filename(prefix, Regex{}))
            res.emplace_back(std::move(filename), "");
    }
    else
    {
        for (auto dir : options["path"].get<StringList>())
        {
            if (not dir.empty() and dir.back() != '/')
                dir += '/';
            for (auto& filename : Kakoune::complete_filename(dir + prefix, Regex{}))
                res.emplace_back(filename.substr(dir.length()).str(), "");
        }
    }
    if (res.empty())
        return {};
    return { begin.coord(), pos.coord(), std::move(res), buffer.timestamp() };
}

InsertCompletion complete_option(const Buffer& buffer, ByteCoord cursor_pos,
                                 OptionManager& options, StringView option_name)
{
    const StringList& opt = options[option_name].get<StringList>();
    if (opt.empty())
        return {};

    auto& desc = opt[0];
    static const Regex re(R"((\d+)\.(\d+)(?:\+(\d+))?@(\d+))");
    MatchResults<String::const_iterator> match;
    if (regex_match(desc.begin(), desc.end(), match, re))
    {
        ByteCoord coord{ str_to_int({match[1].first, match[1].second}) - 1,
                         str_to_int({match[2].first, match[2].second}) - 1 };
        if (not buffer.is_valid(coord))
            return {};
        auto end = coord;
        if (match[3].matched)
        {
            ByteCount len = str_to_int({match[3].first, match[3].second});
            end = buffer.advance(coord, len);
        }
        size_t timestamp = (size_t)str_to_int({match[4].first, match[4].second});
        auto changes = buffer.changes_since(timestamp);
        if (find_if(changes, [&](const Buffer::Change& change){
                        return change.begin < coord;
                    }) != changes.end())
            return {};

        if (cursor_pos.line == coord.line and cursor_pos.column >= coord.column)
        {
            StringView prefix = buffer[coord.line].substr(
                coord.column, cursor_pos.column - coord.column);

            ComplAndDescList res;
            for (auto it = opt.begin() + 1; it != opt.end(); ++it)
            {
                auto splitted = split(*it, '@');
                if (not splitted.empty() and prefix_match(splitted[0], prefix))
                    res.emplace_back(splitted[0].str(), splitted.size() > 1 ? splitted[1].str() : "");
            }
            return { coord, end, std::move(res), timestamp };
        }
    }
    return {};
}

InsertCompletion complete_line(const Buffer& buffer, ByteCoord cursor_pos)
{
    StringView prefix = buffer[cursor_pos.line].substr(0_byte, cursor_pos.column);
    ComplAndDescList res;
    for (LineCount l = 0_line; l < buffer.line_count(); ++l)
    {
        if (l == cursor_pos.line)
            continue;
        ByteCount len = buffer[l].length();
        if (len > cursor_pos.column and std::equal(prefix.begin(), prefix.end(), buffer[l].begin()))
            res.emplace_back(buffer[l].substr(0_byte, len-1).str(), "");
    }
    if (res.empty())
        return {};
    std::sort(res.begin(), res.end());
    res.erase(std::unique(res.begin(), res.end()), res.end());
    return { cursor_pos.line, cursor_pos, std::move(res), buffer.timestamp() };
}

}

InsertCompleter::InsertCompleter(const Context& context)
    : m_context(context), m_options(context.options())
{
    m_options.register_watcher(*this);
}

InsertCompleter::~InsertCompleter()
{
    m_options.unregister_watcher(*this);
}

void InsertCompleter::select(int offset, Vector<Key>& keystrokes)
{
    if (not setup_ifn())
        return;

    auto& buffer = m_context.buffer();
    m_current_candidate = (m_current_candidate + offset) % (int)m_matching_candidates.size();
    if (m_current_candidate < 0)
        m_current_candidate += m_matching_candidates.size();
    const ComplAndDesc& candidate = m_matching_candidates[m_current_candidate];
    auto& selections = m_context.selections();
    const auto& cursor_pos = selections.main().cursor();
    const auto prefix_len = buffer.distance(m_completions.begin, cursor_pos);
    const auto suffix_len = std::max(0_byte, buffer.distance(cursor_pos, m_completions.end));

    auto ref = buffer.string(m_completions.begin, m_completions.end);
    for (auto& sel : selections)
    {
        const auto& cursor = sel.cursor();
        auto pos = buffer.iterator_at(cursor);
        if (cursor.column >= prefix_len and (pos + suffix_len) != buffer.end() and
            std::equal(ref.begin(), ref.end(), pos - prefix_len))
        {
            pos = buffer.erase(pos - prefix_len, pos + suffix_len);
            buffer.insert(pos, candidate.first);
            const_cast<SelectionList&>(selections).update();
        }
    }
    m_completions.end = cursor_pos;
    m_completions.begin = buffer.advance(cursor_pos, -candidate.first.length());
    m_completions.timestamp = buffer.timestamp();
    if (m_context.has_ui())
    {
        m_context.ui().menu_select(m_current_candidate);
        if (not candidate.second.empty())
            m_context.ui().info_show(candidate.first, candidate.second, CharCoord{},
                                     get_face("Information"), InfoStyle::MenuDoc);
    }

    for (auto i = 0_byte; i < prefix_len; ++i)
        keystrokes.push_back(Key::Backspace);
    for (auto i = 0_byte; i < suffix_len; ++i)
        keystrokes.push_back(Key::Delete);
    for (auto& c : candidate.first)
        keystrokes.push_back(c);
}

void InsertCompleter::update()
{
    if (m_completions.is_valid())
    {
        ByteCount longest_completion = 0;
        for (auto& candidate : m_completions.candidates)
             longest_completion = std::max(longest_completion, candidate.first.length());

        ByteCoord cursor = m_context.selections().main().cursor();
        ByteCoord compl_beg = m_completions.begin;
        if (cursor.line == compl_beg.line and
            compl_beg.column <= cursor.column and
            cursor.column < compl_beg.column + longest_completion)
        {
            String prefix = m_context.buffer().string(compl_beg, cursor);

            if (m_context.buffer().timestamp() == m_completions.timestamp)
                m_matching_candidates = m_completions.candidates;
            else
            {
                m_matching_candidates.clear();
                for (auto& candidate : m_completions.candidates)
                {
                    if (candidate.first.substr(0, prefix.length()) == prefix)
                        m_matching_candidates.push_back(candidate);
                }
            }
            if (not m_matching_candidates.empty())
            {
                m_current_candidate = m_matching_candidates.size();
                m_completions.end = cursor;
                menu_show();
                m_matching_candidates.emplace_back(prefix, "");
                return;
            }
        }
    }
    reset();
    setup_ifn();
}

void InsertCompleter::reset()
{
    m_completions = InsertCompletion{};
    if (m_context.has_ui())
    {
        m_context.ui().menu_hide();
        m_context.ui().info_hide();
    }
}

bool InsertCompleter::setup_ifn()
{
    using namespace std::placeholders;
    if (not m_completions.is_valid())
    {
        auto& completers = m_options["completers"].get<InsertCompleterDescList>();
        for (auto& completer : completers)
        {
            if (completer.mode == InsertCompleterDesc::Filename and
                try_complete([this](const Buffer& buffer, ByteCoord cursor_pos) {
                    return complete_filename<true>(buffer, cursor_pos,
                                                   m_options);
                }))
                return true;
            if (completer.mode == InsertCompleterDesc::Option and
                try_complete([&,this](const Buffer& buffer, ByteCoord cursor_pos) {
                   return complete_option(buffer, cursor_pos,
                                          m_options, *completer.param);
                }))
                return true;
            if (completer.mode == InsertCompleterDesc::Word and
                *completer.param == "buffer" and
                (try_complete(complete_word<false, false>) or
                 try_complete(complete_word<false, true>)))
                return true;
            if (completer.mode == InsertCompleterDesc::Word and
                *completer.param == "all" and
                (try_complete(complete_word<true, false>) or
                 try_complete(complete_word<true, true>)))
                return true;
        }
        return false;
    }
    return true;
}

void InsertCompleter::menu_show()
{
    if (not m_context.has_ui())
        return;
    CharCoord menu_pos = m_context.window().display_position(m_completions.begin);

    const CharCount tabstop = m_options["tabstop"].get<int>();
    const CharCount column = get_column(m_context.buffer(), tabstop,
                                        m_completions.begin);
    Vector<String> menu_entries;
    for (auto& candidate : m_matching_candidates)
        menu_entries.push_back(expand_tabs(candidate.first, tabstop, column));

    m_context.ui().menu_show(menu_entries, menu_pos,
                             get_face("MenuForeground"),
                             get_face("MenuBackground"),
                             MenuStyle::Inline);
    m_context.ui().menu_select(m_current_candidate);
}

void InsertCompleter::on_option_changed(const Option& opt)
{
    // Do not reset the menu if the user has selected an entry
    if (not m_matching_candidates.empty() and
        m_current_candidate != m_matching_candidates.size() - 1)
        return;

    auto& completers = m_options["completers"].get<InsertCompleterDescList>();
    for (auto& completer : completers)
    {
        if (completer.mode == InsertCompleterDesc::Option and
            *completer.param == opt.name())
        {
            reset();
            setup_ifn();
            break;
        }
    }
}

template<typename CompleteFunc>
bool InsertCompleter::try_complete(CompleteFunc complete_func)
{
    auto& buffer = m_context.buffer();
    ByteCoord cursor_pos = m_context.selections().main().cursor();
    try
    {
        m_completions = complete_func(buffer, cursor_pos);
    }
    catch (runtime_error& e)
    {
        write_debug(format("error while trying to run completer: {}", e.what()));
        return false;
    }
    if (not m_completions.is_valid())
        return false;

    kak_assert(cursor_pos >= m_completions.begin);
    m_matching_candidates = m_completions.candidates;
    m_current_candidate = m_matching_candidates.size();
    menu_show();
    m_matching_candidates.emplace_back(buffer.string(m_completions.begin, m_completions.end), "");
    return true;
}

void InsertCompleter::explicit_file_complete()
{
    try_complete([this](const Buffer& buffer, ByteCoord cursor_pos) {
        return complete_filename<false>(buffer, cursor_pos, m_options);
    });
}

void InsertCompleter::explicit_word_complete()
{
    try_complete(complete_word<true, true>);
}

void InsertCompleter::explicit_line_complete()
{
    try_complete([this](const Buffer& buffer, ByteCoord cursor_pos) {
        return complete_line(buffer, cursor_pos);
    });
}

}
