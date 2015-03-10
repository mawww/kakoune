#include "word_db.hh"

#include "utils.hh"
#include "line_modification.hh"
#include "utf8_iterator.hh"

namespace Kakoune
{

UsedLetters used_letters(StringView str)
{
    UsedLetters res;
    for (auto c : str)
    {
        if (c >= 'a' and c <= 'z')
            res.set(c - 'a');
        else if (c >= 'A' and c <= 'Z')
            res.set(c - 'A' + 26);
        else if (c == '_')
            res.set(53);
        else if (c == '-')
            res.set(54);
        else
            res.set(63);
    }
    return res;
}

static WordDB::WordList get_words(const SharedString& content)
{
    WordDB::WordList res;
    using Iterator = utf8::iterator<const char*, utf8::InvalidPolicy::Pass>;
    const char* word_start = content.begin();
    bool in_word = false;
    for (Iterator it{word_start}, end{content.end()}; it != end; ++it)
    {
        Codepoint c = *it;
        const bool word = is_word(c);
        if (not in_word and word)
        {
            word_start = it.base();
            in_word = true;
        }
        else if (in_word and not word)
        {
            const ByteCount start = word_start - content.begin();
            const ByteCount length = it.base() - word_start;
            res.push_back(content.acquire_substr(start, length));
            in_word = false;
        }
    }
    return res;
}

void WordDB::add_words(const SharedString& line)
{
    for (auto& w : get_words(line))
    {
        WordDB::WordInfo& info = m_words[intern(w)];
        ++info.refcount;
        if (info.letters.none())
            info.letters = used_letters(w);
    }
}

void WordDB::remove_words(const SharedString& line)
{
    for (auto& w : get_words(line))
    {
        auto it = m_words.find({w, SharedString::NoCopy()});
        kak_assert(it != m_words.end() and it->second.refcount > 0);
        if (--it->second.refcount == 0)
            m_words.erase(it);
    }
}

WordDB::WordDB(const Buffer& buffer)
    : m_buffer{&buffer}, m_timestamp{buffer.timestamp()}
{
    m_lines.reserve((int)buffer.line_count());
    for (auto line = 0_line, end = buffer.line_count(); line < end; ++line)
    {
        m_lines.push_back(buffer.line_storage(line));
        add_words(SharedString{m_lines.back()});
    }
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
            remove_words(SharedString{m_lines[(int)old_line++]});
        }

        for (auto l = 0_line; l < modif.num_added; ++l)
        {
            new_lines.push_back(buffer.line_storage(modif.new_line + l));
            add_words(SharedString{new_lines.back()});
        }
    }
    while (old_line != (int)m_lines.size())
        new_lines.push_back(std::move(m_lines[(int)old_line++]));

    m_lines = std::move(new_lines);
}

int WordDB::get_word_occurences(StringView word) const
{
    auto it = m_words.find({word, SharedString::NoCopy()});
    if (it != m_words.end())
        return it->second.refcount;
    return 0;
}

}
