#include "insert_completer.hh"

#include "buffer_manager.hh"
#include "buffer_utils.hh"
#include "client.hh"
#include "command_manager.hh"
#include "changes.hh"
#include "context.hh"
#include "display_buffer.hh"
#include "face_registry.hh"
#include "file.hh"
#include "regex.hh"
#include "window.hh"
#include "word_db.hh"
#include "option_types.hh"
#include "utf8_iterator.hh"
#include "user_interface.hh"

#include <numeric>
#include <utility>

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
        case InsertCompleterDesc::Line:
            return "line=" + (opt.param ? *opt.param : "");
    }
    kak_assert(false);
    return "";
}

InsertCompleterDesc option_from_string(Meta::Type<InsertCompleterDesc>, StringView str)
{
    if (str.substr(0_byte, 7_byte) == "option=")
        return {InsertCompleterDesc::Option, str.substr(7_byte).str()};
    else if (str.substr(0_byte, 5_byte) == "word=")
    {
        auto param = str.substr(5_byte);
        if (param == "all" or param == "buffer")
            return {InsertCompleterDesc::Word, param.str()};
    }
    else if (str == "filename")
        return {InsertCompleterDesc::Filename, {}};
    else if (str.substr(0_byte, 5_byte) == "line=")
    {
        auto param = str.substr(5_byte);
        if (param == "all" or param == "buffer")
            return {InsertCompleterDesc::Line, param.str()};
    }
    throw runtime_error(format("invalid completer description: '{}'", str));
}

namespace
{

template<bool other_buffers>
InsertCompletion complete_word(const SelectionList& sels,
                               const OptionManager& options,
                               const FaceRegistry& faces)
{
    ConstArrayView<Codepoint> extra_word_chars = options["extra_word_chars"].get<Vector<Codepoint, MemoryDomain::Options>>();
    auto is_word_pred = [extra_word_chars](Codepoint c) { return is_word(c, extra_word_chars); };

    const Buffer& buffer = sels.buffer();
    BufferCoord cursor_pos = sels.main().cursor();

    using Utf8It = utf8::iterator<BufferIterator>;
    Utf8It pos{buffer.iterator_at(cursor_pos), buffer};
    if (pos == buffer.begin() or not is_word_pred(*(pos-1)))
        return {};

    BufferCoord word_begin;
    StringView prefix;
    HashMap<StringView, int> sel_word_counts;
    for (int i = 0; i < sels.size(); ++i)
    {
        int len = 0;
        auto is_short_enough_word = [&] (Codepoint c) { return len++ < WordDB::max_word_len && is_word_pred(c); };

        Utf8It end{buffer.iterator_at(sels[i].cursor()), buffer};
        Utf8It begin = end-1;
        if (not skip_while_reverse(begin, buffer.begin(), is_short_enough_word) and
            begin < end) // (begin might == end if end == buffer.begin())
            ++begin;

        if (i == sels.main_index())
        {
            word_begin = begin.base().coord();
            prefix = buffer.substr(word_begin, end.base().coord());
        }

        skip_while(end, buffer.end(), is_short_enough_word);

        if (len <= WordDB::max_word_len)
        {
            StringView word = buffer.substr(begin.base().coord(), end.base().coord());
            ++sel_word_counts[word];
        }
    }

    struct RankedMatchAndBuffer : RankedMatch
    {
        RankedMatchAndBuffer(RankedMatch  m, const Buffer* b)
            : RankedMatch{std::move(m)}, buffer{b} {}

        using RankedMatch::operator==;
        using RankedMatch::operator<;
        bool operator==(StringView other) const { return candidate() == other; }

        const Buffer* buffer;
    };

    auto& word_db = get_word_db(buffer);
    Vector<RankedMatchAndBuffer> matches = word_db.find_matching(prefix)
                                         | transform([&](auto& m) { return RankedMatchAndBuffer{m, &buffer}; })
                                         | gather<Vector>();
    // Remove words that are being edited
    for (auto& word_count : sel_word_counts)
    {
        if (word_db.get_word_occurences(word_count.key) <= word_count.value)
            unordered_erase(matches, word_count.key);
    }

    if (other_buffers)
    {
        for (const auto& buf : BufferManager::instance())
        {
            if (buf.get() == &buffer or buf->flags() & Buffer::Flags::Debug)
                continue;
            for (auto& m : get_word_db(*buf).find_matching(prefix) |
                           // filter out words that are not considered words for the current buffer
                           filter([&](auto& rm) {
                               auto&& c = rm.candidate();
                               return std::all_of(utf8::iterator{c.begin(), c},
                                                  utf8::iterator{c.end(), c},
                                                  is_word_pred); }))
                matches.push_back({ m, buf.get() });
        }
    }

    using StaticWords = Vector<String, MemoryDomain::Options>;
    for (auto& word : options["static_words"].get<StaticWords>())
        if (RankedMatch match{word, prefix})
            matches.emplace_back(match, nullptr);

    unordered_erase(matches, prefix);
    const auto longest = accumulate(matches, 0_char,
                                    [](const CharCount& lhs, const RankedMatchAndBuffer& rhs)
                                    { return std::max(lhs, rhs.candidate().char_length()); });

    auto limit = [](StringView s, ColumnCount l) { return s.column_length() <= l ? s.str() : "…" + s.substr(s.column_length() - (l + 1)); };
    constexpr size_t max_count = 100;
    // Gather best max_count matches
    InsertCompletion::CandidateList candidates;
    candidates.reserve(std::min(matches.size(), max_count));

    for_n_best(matches, max_count, [](auto& lhs, auto& rhs) { return rhs < lhs; },
               [&](RankedMatchAndBuffer& m) {
        if (not candidates.empty() and candidates.back().completion == m.candidate())
            return false;
        DisplayLine menu_entry;
        if (other_buffers && m.buffer)
        {
            const auto pad_len = longest + 1 - m.candidate().char_length();
            menu_entry.push_back({ m.candidate().str(), {} });
            menu_entry.push_back({ String{' ', pad_len}, {} });
            menu_entry.push_back({ limit(m.buffer->display_name(), 20), faces["MenuInfo"] });
        }
        else
            menu_entry.push_back({ m.candidate().str(), {} });

        candidates.push_back({m.candidate().str(), "", std::move(menu_entry)});
        return true;
    });

    return { std::move(candidates), word_begin, cursor_pos, buffer.timestamp() };
}

template<bool require_slash>
InsertCompletion complete_filename(const SelectionList& sels,
                                   const OptionManager& options,
                                   const FaceRegistry&)
{
    const Buffer& buffer = sels.buffer();
    auto pos = buffer.iterator_at(sels.main().cursor());
    auto begin = pos;

    auto is_filename = [](char c)
    {
        return isalnum(c) or c == '/' or c == '.' or c == '_' or c == '-';
    };
    while (begin != buffer.begin() and is_filename(*(begin-1)))
        --begin;

    if (begin != buffer.begin() and *begin == '/' and *(begin-1) == '~')
        --begin;

    StringView prefix = buffer.substr(begin.coord(), pos.coord());
    if (require_slash and not contains(prefix, '/'))
        return {};

    // Do not try to complete in that case as its unlikely to be a filename,
    // and triggers network host search of cygwin.
    if (prefix.substr(0_byte, 2_byte) == "//")
        return {};

    InsertCompletion::CandidateList candidates;
    if (prefix.front() == '/' or prefix.front() == '~')
    {
        for (auto& filename : Kakoune::complete_filename(prefix,
                                                         options["ignored_files"].get<Regex>()))
            candidates.push_back({ filename, "", {filename, {}} });
    }
    else
    {
        Vector<String> visited_dirs;
        for (auto dir : options["path"].get<StringList>())
        {
            dir = real_path(parse_filename(dir, (buffer.flags() & Buffer::Flags::File) ?
                                           split_path(buffer.name()).first : StringView{}));

            if (not dir.empty() and dir.back() != '/')
                dir += '/';

            if (contains(visited_dirs, dir))
                continue;

            for (auto& filename : Kakoune::complete_filename(dir + prefix,
                                                             options["ignored_files"].get<Regex>()))
            {
                StringView candidate = filename.substr(dir.length());
                candidates.push_back({ candidate.str(), "", {candidate.str(), {}} });
            }

            visited_dirs.push_back(std::move(dir));
        }
    }
    if (candidates.empty())
        return {};
    return { std::move(candidates), begin.coord(), pos.coord(), buffer.timestamp() };
}

InsertCompletion complete_option(const SelectionList& sels,
                                 const OptionManager& options,
                                 const FaceRegistry& faces,
                                 StringView option_name)
{
    const Buffer& buffer = sels.buffer();
    BufferCoord cursor_pos = sels.main().cursor();

    const CompletionList& opt = options[option_name].get<CompletionList>();
    if (opt.list.empty())
        return {};

    auto& desc = opt.prefix;
    static const Regex re(R"((\d+)\.(\d+)(?:\+(\d+))?@(\d+))");
    MatchResults<String::const_iterator> match;
    if (not regex_match(desc.begin(), desc.end(), match, re))
        return {};

    BufferCoord coord{str_to_int({match[1].first, match[1].second}) - 1,
                      str_to_int({match[2].first, match[2].second}) - 1};
    if (not buffer.is_valid(coord))
        return {};
    size_t timestamp = (size_t)str_to_int({match[4].first, match[4].second});
    auto changes = buffer.changes_since(timestamp);
    if (any_of(changes, [&](auto&& change) { return change.begin < coord; }))
        return {};

    if (cursor_pos.line != coord.line or cursor_pos.column < coord.column)
        return {};

    const ColumnCount tabstop = options["tabstop"].get<int>();
    const ColumnCount column = get_column(buffer, tabstop, cursor_pos);

    struct RankedMatchAndInfo : RankedMatch
    {
        using RankedMatch::RankedMatch;
        using RankedMatch::operator==;
        using RankedMatch::operator<;

        StringView on_select;
        DisplayLine menu_entry;
    };

    StringView query = buffer.substr(coord, cursor_pos);
    Vector<RankedMatchAndInfo> matches;

    for (auto& candidate : opt.list)
    {
        if (RankedMatchAndInfo match{std::get<0>(candidate), query})
        {
            match.on_select = std::get<1>(candidate);
            auto& menu = std::get<2>(candidate);
            match.menu_entry = not menu.empty() ?
                parse_display_line(expand_tabs(menu, tabstop, column), faces)
              : DisplayLine{String{}, {}};

            matches.push_back(std::move(match));
        }
    }

    constexpr size_t max_count = 100;
    // Gather best max_count matches
    auto greater = [](auto& lhs, auto& rhs) { return rhs < lhs; };
    auto first = matches.begin(), last = matches.end();
    std::make_heap(first, last, greater);
    InsertCompletion::CandidateList candidates;
    candidates.reserve(std::min(matches.size(), max_count));
    candidates.reserve(matches.size());
    while (candidates.size() < max_count and first != last)
    {
        if (candidates.empty() or candidates.back().completion != first->candidate()
            or candidates.back().on_select != first->on_select)
            candidates.push_back({ first->candidate().str(), first->on_select.str(),
                                   std::move(first->menu_entry) });
        std::pop_heap(first, last--, greater);
    }

    auto end = cursor_pos;
    if (match[3].matched)
    {
        ByteCount len = str_to_int({match[3].first, match[3].second});
        end = buffer.advance(coord, len);
    }
    return { std::move(candidates), coord, end, timestamp };
}

template<bool other_buffers>
InsertCompletion complete_line(const SelectionList& sels,
                               const OptionManager& options,
                               const FaceRegistry&)
{
    const Buffer& buffer = sels.buffer();
    BufferCoord cursor_pos = sels.main().cursor();

    const ColumnCount tabstop = options["tabstop"].get<int>();
    const ColumnCount column = get_column(buffer, tabstop, cursor_pos);

    auto trim_leading_whitespaces = [](StringView s) {
        utf8::iterator it{s.begin(), s};
        while (it != s.end() and is_horizontal_blank(*it))
            ++it;
        return StringView{it.base(), s.end()};
    };

    StringView prefix = trim_leading_whitespaces(buffer[cursor_pos.line].substr(0_byte, cursor_pos.column));
    BufferCoord replace_begin = buffer.advance(cursor_pos, -prefix.length());
    InsertCompletion::CandidateList candidates;

    auto add_candidates = [&](const Buffer& buf) {
        for (LineCount l = 0_line; l < buf.line_count(); ++l)
        {
            if (buf.name() == buffer.name() && l == cursor_pos.line)
                continue;

            StringView line = buf[l];
            StringView candidate = trim_leading_whitespaces(line.substr(0_byte, line.length()-1));

            if (candidate.length() == 0)
              continue;

            if (prefix == candidate.substr(0_byte, prefix.length()))
            {
                candidates.push_back({candidate.str(), "", {expand_tabs(candidate, tabstop, column), {}} });
                // perf: it's unlikely the user intends to search among >10 candidates anyway
                if (candidates.size() == 100)
                    break;
            }
        }
    };

    add_candidates(buffer);

    if (other_buffers)
    {
        for (const auto& buf : BufferManager::instance())
        {
            if (buf.get() != &buffer and not (buf->flags() & Buffer::Flags::Debug))
                add_candidates(*buf);
        }
    }

    if (candidates.empty())
        return {};
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    return { std::move(candidates), replace_begin, cursor_pos, buffer.timestamp() };
}

}

InsertCompleter::InsertCompleter(Context& context)
    : m_context(context), m_options(context.options()), m_faces(context.faces())
{
    m_options.register_watcher(*this);
}

InsertCompleter::~InsertCompleter()
{
    m_options.unregister_watcher(*this);
}

void InsertCompleter::select(int index, bool relative, Vector<Key>& keystrokes)
{
    m_enabled = true;
    if (not setup_ifn())
        return;

    auto& buffer = m_context.buffer();
    m_current_candidate = (relative ? m_current_candidate + index : index) % (int)m_completions.candidates.size();
    if (m_current_candidate < 0)
        m_current_candidate += m_completions.candidates.size();
    const InsertCompletion::Candidate& candidate = m_completions.candidates[m_current_candidate];
    auto& selections = m_context.selections();
    const auto& cursor_pos = selections.main().cursor();
    const auto prefix_len = buffer.distance(m_completions.begin, cursor_pos);
    const auto suffix_len = std::max(0_byte, buffer.distance(cursor_pos, m_completions.end));

    auto ref = buffer.string(m_completions.begin, m_completions.end);
    Vector<BufferRange> ranges;
    for (auto& sel : selections)
    {
        auto pos = buffer.iterator_at(sel.cursor());
        if (pos.coord().column >= prefix_len and (pos + suffix_len) != buffer.end() and std::equal(ref.begin(), ref.end(), pos - prefix_len))
            ranges.push_back({(pos - prefix_len).coord(), (pos + suffix_len).coord()});
    }
    replace(buffer, ranges, candidate.completion);

    selections.update();
    m_completions.end = cursor_pos;
    m_completions.begin = buffer.advance(cursor_pos, -candidate.completion.length());
    m_completions.timestamp = buffer.timestamp();
    m_inserted_ranges = std::move(ranges);

    if (m_context.has_client())
    {
        m_context.client().menu_select(m_current_candidate);
    }

    for (auto i = 0_byte; i < prefix_len; ++i)
        keystrokes.emplace_back(Key::Backspace);
    for (auto i = 0_byte; i < suffix_len; ++i)
        keystrokes.emplace_back(Key::Delete);
    for (auto& c : candidate.completion)
        keystrokes.emplace_back(c);

    if (not candidate.on_select.empty())
        CommandManager::instance().execute(candidate.on_select, m_context);
}

void InsertCompleter::update(bool allow_implicit)
{
    m_enabled = allow_implicit;
    if (m_explicit_completer and try_complete(m_explicit_completer))
        return;

    reset();
    setup_ifn();
}

auto& get_first(BufferRange& range) { return range.begin; }
auto& get_last(BufferRange& range) { return range.end; }

bool InsertCompleter::has_candidate_selected() const
{
    return m_current_candidate >= 0 and m_current_candidate < m_completions.candidates.size() - 1;
}

void InsertCompleter::try_accept()
{
    if (m_completions.is_valid() and m_context.has_client() and has_candidate_selected())
        reset();
}

void InsertCompleter::reset()
{
    if (not m_explicit_completer and not m_completions.is_valid())
        return;

    String hook_param;
    if (m_context.has_client() and has_candidate_selected())
    {
        auto& buffer = m_context.buffer();
        update_ranges(buffer, m_completions.timestamp, m_inserted_ranges);
        hook_param = join(m_inserted_ranges | filter([](auto&& r) { return not r.empty(); }) | transform([&](auto&& r) {
                return selection_to_string(ColumnType::Byte, buffer, {r.begin, buffer.char_prev(r.end)});
            }), ' ');
    }

    m_explicit_completer = nullptr;
    m_completions = InsertCompletion{};
    m_inserted_ranges.clear();
    if (m_context.has_client())
    {
        m_context.client().menu_hide();
        m_context.client().info_hide();
        m_context.hooks().run_hook(Hook::InsertCompletionHide, hook_param, m_context);
    }
}

bool InsertCompleter::setup_ifn()
{
    if (!m_enabled)
        return false;
    using namespace std::placeholders;
    if (not m_completions.is_valid())
    {
        auto& completers = m_options["completers"].get<InsertCompleterDescList>();
        for (auto& completer : completers)
        {
            if (completer.mode == InsertCompleterDesc::Filename and
                try_complete(complete_filename<true>))
                return true;
            if (completer.mode == InsertCompleterDesc::Option and
                try_complete([&](const SelectionList& sels,
                                 const OptionManager& options,
                                 const FaceRegistry& faces) {
                   return complete_option(sels, options, faces, *completer.param);
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
            if (completer.mode == InsertCompleterDesc::Line and
                *completer.param == "buffer" and
                try_complete(complete_line<false>))
                return true;
            if (completer.mode == InsertCompleterDesc::Line and
                *completer.param == "all" and
                try_complete(complete_line<true>))
                return true;
        }
        return false;
    }
    return true;
}

void InsertCompleter::menu_show()
{
    if (not m_context.has_client())
        return;

    Vector<DisplayLine> menu_entries;
    for (auto& candidate : m_completions.candidates)
        menu_entries.push_back(candidate.menu_entry);

    m_context.client().menu_show(std::move(menu_entries), m_completions.begin,
                                 MenuStyle::Inline);
    m_context.client().menu_select(m_current_candidate);
    m_context.hooks().run_hook(Hook::InsertCompletionShow, "", m_context);
}

void InsertCompleter::on_option_changed(const Option& opt)
{
    // Do not reset the menu if the user has selected an entry
    if (not m_completions.candidates.empty() and
        m_current_candidate != m_completions.candidates.size() - 1)
        return;

    const auto& completers = m_options["completers"].get<InsertCompleterDescList>();
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

template<typename Func>
bool InsertCompleter::try_complete(Func complete_func)
{
    auto& sels = m_context.selections();
    try
    {
        reset();
        m_completions = complete_func(sels, m_options, m_faces);
    }
    catch (runtime_error& e)
    {
        write_to_debug_buffer(format("error while trying to run completer: {}", e.what()));
        return false;
    }
    if (not m_completions.is_valid())
        return false;

    kak_assert(m_completions.begin <= sels.main().cursor());
    m_current_candidate = m_completions.candidates.size();
    menu_show();
    m_completions.candidates.push_back({sels.buffer().string(m_completions.begin, m_completions.end), "", {}});
    return true;
}

void InsertCompleter::explicit_file_complete()
{
    try_complete(complete_filename<false>);
    m_explicit_completer = complete_filename<false>;
}

void InsertCompleter::explicit_word_buffer_complete()
{
    try_complete(complete_word<false>);
    m_explicit_completer = complete_word<false>;
}

void InsertCompleter::explicit_word_all_complete()
{
    try_complete(complete_word<true>);
    m_explicit_completer = complete_word<true>;
}

void InsertCompleter::explicit_line_buffer_complete()
{
    try_complete(complete_line<false>);
    m_explicit_completer = complete_line<false>;
}

void InsertCompleter::explicit_line_all_complete()
{
    try_complete(complete_line<true>);
    m_explicit_completer = complete_line<true>;
}

}
