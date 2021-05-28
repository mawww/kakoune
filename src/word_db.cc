#include "word_db.hh"

#include "buffer.hh"
#include "line_modification.hh"
#include "option_types.hh"
#include "unit_tests.hh"
#include "utils.hh"
#include "value.hh"

namespace Kakoune
{

WordDB& get_word_db(const Buffer& buffer)
{
    static const ValueId word_db_id = get_free_value_id();
    Value& cache_val = buffer.values()[word_db_id];
    if (not cache_val)
        cache_val = Value(WordDB{buffer});
    return cache_val.as<WordDB>();
}

struct WordSplitter
{
    struct Iterator
    {
        Iterator(const char* begin, const WordSplitter& splitter)
            : m_word_begin{begin}, m_word_end{begin}, m_splitter{&splitter}
        { operator++(); }

        StringView operator*() const { return {m_word_begin, m_word_end}; }

        Iterator& operator++()
        {
            const auto* end = m_splitter->m_content.end();
            auto extra_chars = m_splitter->m_extra_word_chars;

            while (true)
            {
                m_word_begin = m_word_end;
                while (m_word_begin != end and not is_word(utf8::codepoint(m_word_begin, end), extra_chars))
                    utf8::to_next(m_word_begin, end);
                m_word_end = m_word_begin;
                CharCount word_len = 0;
                while (m_word_end != end and is_word(utf8::codepoint(m_word_end, end), extra_chars))
                {
                    utf8::to_next(m_word_end, end);
                    ++word_len;
                }
                if (m_word_begin == end or word_len < WordDB::max_word_len)
                    break;
            }

            return *this;
        }

        friend bool operator==(const Iterator& lhs, const Iterator& rhs)
        { return lhs.m_word_begin == rhs.m_word_begin and lhs.m_word_end == rhs.m_word_end; }

        friend bool operator!=(const Iterator& lhs, const Iterator& rhs)
        { return not (lhs == rhs); }

        const char* m_word_begin;
        const char* m_word_end;
        const WordSplitter* m_splitter;
    };

    StringView m_content;
    ConstArrayView<Codepoint> m_extra_word_chars;

    Iterator begin() const { return {m_content.begin(), *this}; }
    Iterator end()   const { return {m_content.end(), *this}; }
};

static ConstArrayView<Codepoint> get_extra_word_chars(const Buffer& buffer)
{
    return buffer.options()["extra_word_chars"].get<Vector<Codepoint, MemoryDomain::Options>>();
}

void WordDB::add_words(StringView line, ConstArrayView<Codepoint> extra_word_chars)
{
    for (auto&& w : WordSplitter{line, extra_word_chars})
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

void WordDB::remove_words(StringView line, ConstArrayView<Codepoint> extra_word_chars)
{
    for (auto&& w : WordSplitter{line, extra_word_chars})
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
    auto extra_word_chars = get_extra_word_chars(buffer);
    for (auto line = 0_line, end = buffer.line_count(); line < end; ++line)
    {
        m_lines.push_back(buffer.line_storage(line));
        add_words(m_lines.back()->strview(), extra_word_chars);
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

    auto extra_word_chars = get_extra_word_chars(buffer);
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
            remove_words(m_lines[(int)old_line++]->strview(), extra_word_chars);
        }

        for (auto l = 0_line; l < modif.num_added; ++l)
        {
            new_lines.push_back(buffer.line_storage(modif.new_line + l));
            add_words(new_lines.back()->strview(), extra_word_chars);
        }
    }
    while (old_line != (int)m_lines.size())
        new_lines.push_back(std::move(m_lines[(int)old_line++]));

    m_lines = std::move(new_lines);
}

void WordDB::on_option_changed(const Option& option)
{
    if (option.name() == "extra_word_chars")
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

    using WordList = Vector<StringView>;
    auto eq = [](ArrayView<const RankedMatch> lhs, const WordList& rhs) {
        return lhs.size() == rhs.size() and
            std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                       [](const RankedMatch& lhs, const StringView& rhs) {
                           return lhs.candidate() == rhs;
                       });
    };

    auto make_lines = [](auto&&... lines) { return BufferLines{StringData::create({lines})...}; };

    Buffer buffer("test", Buffer::Flags::None,
                  make_lines("tchou mutch\n", "tchou kanaky tchou\n", "\n", "tchaa tchaa\n", "allo\n"));
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
