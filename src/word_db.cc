#include "word_db.hh"

#include "utils.hh"
#include "line_modification.hh"
#include "utf8_iterator.hh"
#include "unit_tests.hh"

namespace Kakoune
{

using WordList = Vector<StringView>;

static WordList get_words(StringView content, StringView extra_word_chars)
{
    WordList res;
    using Utf8It = utf8::iterator<const char*>;
    const char* word_start = content.begin();
    bool in_word = false;
    for (Utf8It it{word_start, content}, end{content.end(), content}; it != end; ++it)
    {
        Codepoint c = *it;
        const bool word = is_word(c) or contains(extra_word_chars, c);
        if (not in_word and word)
        {
            word_start = it.base();
            in_word = true;
        }
        else if (in_word and not word)
        {
            const ByteCount start = word_start - content.begin();
            const ByteCount length = it.base() - word_start;
            res.push_back(content.substr(start, length));
            in_word = false;
        }
    }
    return res;
}

static StringView get_extra_word_chars(const Buffer& buffer)
{
    return buffer.options()["completion_extra_word_char"].get<String>();
}

void WordDB::add_words(StringView line)
{
    for (auto& w : get_words(line, get_extra_word_chars(*m_buffer)))
    {
        auto it = m_words.find(w);
        if (it == m_words.end())
        {
            auto word = intern(w);
            WordDB::WordInfo& info = m_words[word->strview()];
            info.word = word;
            info.letters = used_letters(w);
            ++info.refcount;
        }
        else
            ++ it->second.refcount;
    }
}

void WordDB::remove_words(StringView line)
{
    for (auto& w : get_words(line, get_extra_word_chars(*m_buffer)))
    {
        auto it = m_words.find(w);
        kak_assert(it != m_words.end() and it->second.refcount > 0);
        if (--it->second.refcount == 0)
            m_words.erase(it);
    }
}

WordDB::WordDB(const Buffer& buffer)
    : m_buffer{&buffer}
{
    buffer.options().register_watcher(*this);
    rebuild_db();
}

WordDB::WordDB(WordDB&& other)
    : m_buffer{std::move(other.m_buffer)},
      m_lines{std::move(other.m_lines)},
      m_words{std::move(other.m_words)},
      m_timestamp{other.m_timestamp}
{
    kak_assert(m_buffer);
    m_buffer->options().unregister_watcher(other);
    other.m_buffer = nullptr;

    m_buffer->options().register_watcher(*this);
}

WordDB::~WordDB()
{
    if (m_buffer)
        m_buffer->options().unregister_watcher(*this);
}

void WordDB::rebuild_db()
{
    auto& buffer = *m_buffer;

    m_words.clear();
    m_lines.clear();
    m_lines.reserve((int)buffer.line_count());
    for (auto line = 0_line, end = buffer.line_count(); line < end; ++line)
    {
        m_lines.push_back(buffer.line_storage(line));
        add_words(m_lines.back()->strview());
    }
    m_timestamp = buffer.timestamp();
}

void WordDB::update_db()
{
    auto& buffer = *m_buffer;

    auto modifs = compute_line_modifications(buffer, m_timestamp);
    m_timestamp = buffer.timestamp();

    if (modifs.empty())
        return;

    Lines new_lines;
    new_lines.reserve((int)buffer.line_count());

    auto old_line = 0_line;
    for (auto& modif : modifs)
    {
        kak_assert(0_line <= modif.new_line and modif.new_line <= buffer.line_count());
        kak_assert(modif.new_line < buffer.line_count() or modif.num_added == 0);
        kak_assert(old_line <= modif.old_line);
        while (old_line < modif.old_line)
            new_lines.push_back(std::move(m_lines[(int)old_line++]));

        kak_assert((int)new_lines.size() == (int)modif.new_line);

        while (old_line < modif.old_line + modif.num_removed)
        {
            kak_assert(old_line < m_lines.size());
            remove_words(m_lines[(int)old_line++]->strview());
        }

        for (auto l = 0_line; l < modif.num_added; ++l)
        {
            new_lines.push_back(buffer.line_storage(modif.new_line + l));
            add_words(new_lines.back()->strview());
        }
    }
    while (old_line != (int)m_lines.size())
        new_lines.push_back(std::move(m_lines[(int)old_line++]));

    m_lines = std::move(new_lines);
}

void WordDB::on_option_changed(const Option& option)
{
    if (option.name() == "completion_extra_word_char")
        rebuild_db();
}

int WordDB::get_word_occurences(StringView word) const
{
    auto it = m_words.find(word);
    if (it != m_words.end())
        return it->second.refcount;
    return 0;
}

RankedMatchList WordDB::find_matching(StringView query)
{
    update_db();
    const UsedLetters letters = used_letters(query);
    RankedMatchList res;
    for (auto&& word : m_words)
    {
        if (RankedMatch match{word.first, word.second.letters, query, letters})
            res.push_back(match);
    }

    return res;
}

UnitTest test_word_db{[]()
{
    auto cmp_words = [](const RankedMatch& lhs, const RankedMatch& rhs) {
        return lhs.candidate() < rhs.candidate();
    };

    auto eq = [](ArrayView<const RankedMatch> lhs, const WordList& rhs) {
        return lhs.size() == rhs.size() and
            std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                       [](const RankedMatch& lhs, const StringView& rhs) {
                           return lhs.candidate() == rhs;
                       });
    };

    Buffer buffer("test", Buffer::Flags::None,
                  "tchou mutch\n"
                  "tchou kanaky tchou\n"
                  "\n"
                  "tchaa tchaa\n"
                  "allo\n");
    WordDB word_db(buffer);
    auto res = word_db.find_matching("");
    std::sort(res.begin(), res.end(), cmp_words);
    kak_assert(eq(res, WordList{ "allo", "kanaky", "mutch", "tchaa", "tchou" }));
    kak_assert(word_db.get_word_occurences("tchou") == 3);
    kak_assert(word_db.get_word_occurences("allo") == 1);
    buffer.erase({1, 6}, {4, 0});
    res = word_db.find_matching("");
    std::sort(res.begin(), res.end(), cmp_words);
    kak_assert(eq(res, WordList{ "allo", "mutch", "tchou" }));
    buffer.insert({1, 0}, "re");
    res = word_db.find_matching("");
    std::sort(res.begin(), res.end(), cmp_words);
    kak_assert(eq(res, WordList{ "allo", "mutch", "retchou", "tchou" }));
}};

}
