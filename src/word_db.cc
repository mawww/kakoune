#include "word_db.hh"

#include "utils.hh"
#include "line_modification.hh"
#include "utf8_iterator.hh"
#include "unit_tests.hh"

namespace Kakoune
{

using WordList = Vector<StringView>;

static WordList get_words(StringView content, ConstArrayView<Codepoint> extra_word_chars)
{
    WordList res;
    auto is_word = [&](Codepoint c) {
        return Kakoune::is_word(c) or contains(extra_word_chars, c);
    };
    for (utf8::iterator<const char*> it{content.begin(), content};
         it != content.end(); ++it)
    {
        if (is_word(*it))
        {
            const char* word = it.base(); 
            while (++it != content.end() and is_word(*it))
            {}
            res.emplace_back(word, it.base());
        }
    }
    return res;
}

static Vector<Codepoint> get_extra_word_chars(const Buffer& buffer)
{
    auto& str = buffer.options()["completion_extra_word_char"].get<String>();
    Vector<Codepoint> res;
    for (utf8::iterator<const char*> it{str.begin(), str}; it != str.end(); ++it)
        res.push_back(*it);
    return res;
}

void WordDB::add_words(StringView line)
{
    for (auto& w : get_words(line, get_extra_word_chars(*m_buffer)))
    {
        auto it = m_words.find(w);
        if (it != m_words.end())
            ++it->value.refcount;
        else
        {
            auto word = intern(w);
            auto view = word->strview();
            m_words.insert({view, {std::move(word), used_letters(view), 1}});
        }
    }
}

void WordDB::remove_words(StringView line)
{
    for (auto& w : get_words(line, get_extra_word_chars(*m_buffer)))
    {
        auto it = m_words.find(w);
        kak_assert(it != m_words.end() and it->value.refcount > 0);
        if (--it->value.refcount == 0)
            m_words.unordered_remove(it->key);
    }
}

WordDB::WordDB(const Buffer& buffer)
    : m_buffer{&buffer}
{
    buffer.options().register_watcher(*this);
    rebuild_db();
}

WordDB::WordDB(WordDB&& other) noexcept
    : m_buffer{std::move(other.m_buffer)},
      m_timestamp{other.m_timestamp},
      m_words{std::move(other.m_words)},
      m_lines{std::move(other.m_lines)}
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
        return it->value.refcount;
    return 0;
}

RankedMatchList WordDB::find_matching(StringView query)
{
    update_db();
    const UsedLetters letters = used_letters(query);
    RankedMatchList res;
    for (auto&& word : m_words)
    {
        if (RankedMatch match{word.key, word.value.letters, query, letters})
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
