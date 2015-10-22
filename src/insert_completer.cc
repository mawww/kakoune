#include "insert_completer.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "context.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
#include "file.hh"
#include "regex.hh"
#include "user_interface.hh"
#include "window.hh"
#include "word_db.hh"

#include <numeric>

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
    throw runtime_error(format("invalid completer description: '{}'", str));
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

template<bool other_buffers>
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

    struct RankedMatchAndBuffer : RankedMatch
    {
        RankedMatchAndBuffer(StringView w, int r = 0, const Buffer* b = nullptr)
            : RankedMatch{w, r}, buffer{b} {}

        bool operator==(const RankedMatchAndBuffer& other) const { return word == other.word; }
        bool operator<(const RankedMatchAndBuffer& other) const { return rank > other.rank; }

        const Buffer* buffer;
    };
    Vector<RankedMatchAndBuffer> matches;

    auto add_matches = [&](const Buffer& buf) {
        auto& word_db = get_word_db(buf);
        auto bufmatches = word_db.find_matching(prefix);
        for (auto& m : bufmatches)
            matches.push_back({ m.word, m.rank, &buf });
    };

    add_matches(buffer);

    if (get_word_db(buffer).get_word_occurences(current_word) <= 1)
        unordered_erase(matches, StringView{current_word});

    if (other_buffers)
    {
        for (const auto& buf : BufferManager::instance())
        {
            if (buf.get() == &buffer)
                continue;
            add_matches(*buf);
        }
    }
    unordered_erase(matches, StringView{prefix});
    // Sort by word, favoring lowercase
    std::sort(matches.begin(), matches.end(),
              [](const RankedMatchAndBuffer& lhs, const RankedMatchAndBuffer& rhs) {
                  return std::lexicographical_compare(
                      lhs.word.begin(), lhs.word.end(), rhs.word.begin(), rhs.word.end(),
                      [](char a, char b) {
                          const bool low_a = islower(a), low_b = islower(b);
                          return low_a == low_b ? a < b : low_a;
                      });
              });
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    // Stable sort by rank to preserve by word sorting
    std::stable_sort(matches.begin(), matches.end());

    const auto longest = std::accumulate(matches.begin(), matches.end(), 0_char,
                                         [](const CharCount& lhs, const RankedMatchAndBuffer& rhs)
                                         { return std::max(lhs, rhs.word.char_length()); });

    InsertCompletion::CandidateList candidates;
    candidates.reserve(matches.size());
    for (auto& m : matches)
    {
        DisplayLine menu_entry;
        if (m.buffer)
        {
            const auto pad_len = longest + 1 - m.word.char_length();
            menu_entry.push_back(m.word.str());
            menu_entry.push_back(String{' ', pad_len});
            menu_entry.push_back({ m.buffer->display_name(), get_face("MenuInfo") });
        }
        else
            menu_entry.push_back(m.word.str());

        menu_entry.push_back({ " " + to_string(m.rank), get_face("cyan") });

        candidates.push_back({m.word.str(), "", std::move(menu_entry)});
    }

    return { begin.coord(), cursor_pos, std::move(candidates), buffer.timestamp() };
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

    InsertCompletion::CandidateList candidates;
    if (prefix.front() == '/')
    {
        for (auto& filename : Kakoune::complete_filename(prefix, Regex{}))
            candidates.push_back({ filename, "", filename });
    }
    else
    {
        for (auto dir : options["path"].get<StringList>())
        {
            if (not dir.empty() and dir.back() != '/')
                dir += '/';
            for (auto& filename : Kakoune::complete_filename(dir + prefix, Regex{}))
            {
                StringView candidate = filename.substr(dir.length());
                candidates.push_back({ candidate.str(), "", candidate.str() });
            }
        }
    }
    if (candidates.empty())
        return {};
    return { begin.coord(), pos.coord(), std::move(candidates), buffer.timestamp() };
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


            const CharCount tabstop = options["tabstop"].get<int>();
            const CharCount column = get_column(buffer, tabstop, cursor_pos);

            InsertCompletion::CandidateList candidates;
            for (auto it = opt.begin() + 1; it != opt.end(); ++it)
            {
                auto splitted = split(*it, '@');
                if (not splitted.empty() and prefix_match(splitted[0], prefix))
                {
                    StringView completion = splitted[0];
                    StringView docstring = splitted.size() > 1 ? splitted[1] : StringView{};

                    DisplayLine menu_entry = splitted.size() > 2 ?
                        parse_display_line(expand_tabs(splitted[2], tabstop, column))
                      : DisplayLine{ expand_tabs(splitted[0], tabstop, column) };

                    candidates.push_back({completion.str(), docstring.str(), std::move(menu_entry)});
                }
            }
            return { coord, end, std::move(candidates), timestamp };
        }
    }
    return {};
}

InsertCompletion complete_line(const Buffer& buffer, OptionManager& options, ByteCoord cursor_pos)
{
    const CharCount tabstop = options["tabstop"].get<int>();
    const CharCount column = get_column(buffer, tabstop, cursor_pos);

    StringView prefix = buffer[cursor_pos.line].substr(0_byte, cursor_pos.column);
    InsertCompletion::CandidateList candidates;
    for (LineCount l = 0_line; l < buffer.line_count(); ++l)
    {
        if (l == cursor_pos.line)
            continue;
        ByteCount len = buffer[l].length();
        if (len > cursor_pos.column and std::equal(prefix.begin(), prefix.end(), buffer[l].begin()))
        {
            StringView candidate = buffer[l].substr(0_byte, len-1);
            candidates.push_back({candidate.str(), "",
                                  expand_tabs(candidate, tabstop, column)});
        }
    }
    if (candidates.empty())
        return {};
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    return { cursor_pos.line, cursor_pos, std::move(candidates), buffer.timestamp() };
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
    const InsertCompletion::Candidate& candidate = m_matching_candidates[m_current_candidate];
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
            buffer.insert(pos, candidate.completion);
            const_cast<SelectionList&>(selections).update();
        }
    }
    m_completions.end = cursor_pos;
    m_completions.begin = buffer.advance(cursor_pos, -candidate.completion.length());
    m_completions.timestamp = buffer.timestamp();
    if (m_context.has_ui())
    {
        m_context.ui().menu_select(m_current_candidate);
        if (not candidate.docstring.empty())
            m_context.ui().info_show(candidate.completion, candidate.docstring, CharCoord{},
                                     get_face("Information"), InfoStyle::MenuDoc);
    }

    for (auto i = 0_byte; i < prefix_len; ++i)
        keystrokes.push_back(Key::Backspace);
    for (auto i = 0_byte; i < suffix_len; ++i)
        keystrokes.push_back(Key::Delete);
    for (auto& c : candidate.completion)
        keystrokes.push_back(c);
}

void InsertCompleter::update()
{
    if (m_explicit_completer and try_complete(m_explicit_completer))
        return;

    reset();
    setup_ifn();
}

void InsertCompleter::reset()
{
    m_completions = InsertCompletion{};
    m_explicit_completer = nullptr;
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
                try_complete(complete_word<false>))
                return true;
            if (completer.mode == InsertCompleterDesc::Word and
                *completer.param == "all" and
                try_complete(complete_word<true>))
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

    Vector<DisplayLine> menu_entries;
    for (auto& candidate : m_matching_candidates)
        menu_entries.push_back(candidate.menu_entry);

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
        write_to_debug_buffer(format("error while trying to run completer: {}", e.what()));
        return false;
    }
    if (not m_completions.is_valid())
        return false;

    kak_assert(cursor_pos >= m_completions.begin);
    m_matching_candidates = m_completions.candidates;
    m_current_candidate = m_matching_candidates.size();
    menu_show();
    m_matching_candidates.push_back({buffer.string(m_completions.begin, m_completions.end), ""});
    return true;
}

void InsertCompleter::explicit_file_complete()
{
    auto func = [this](const Buffer& buffer, ByteCoord cursor_pos) {
        return complete_filename<false>(buffer, cursor_pos, m_options);
    };
    try_complete(func);
    m_explicit_completer = func;
}

void InsertCompleter::explicit_word_complete()
{
    try_complete(complete_word<true>);
    m_explicit_completer = complete_word<true>;
}

void InsertCompleter::explicit_line_complete()
{
    auto func = [this](const Buffer& buffer, ByteCoord cursor_pos) {
        return complete_line(buffer, m_options, cursor_pos);
    };
    try_complete(func);
    m_explicit_completer = func;
}

}
