#include "word_db.hh"

#include "utils.hh"
#include "utf8_iterator.hh"

namespace Kakoune
{

WordDB::WordDB(const Buffer& buffer)
    : BufferChangeListener_AutoRegister{const_cast<Buffer&>(buffer)}
{
    for (auto line = 0_line, end = buffer.line_count(); line < end; ++line)
        add_words(line, buffer[line]);
}

void WordDB::add_words(LineCount line, const String& content)
{
    using Iterator = utf8::utf8_iterator<String::const_iterator,
                                         utf8::InvalidBytePolicy::Pass>;
    auto word_start = content.begin();
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
            String w{word_start, it.base()};
            m_word_to_lines[w].push_back(line);
            m_line_to_words[line].push_back(w);
            in_word = false;
        }
    }
}

WordDB::LineToWords::iterator WordDB::remove_line(LineToWords::iterator it)
{
    if (it == m_line_to_words.end())
        return it;

    for (auto& word : it->second)
    {
        auto wtl_it = m_word_to_lines.find(word);
        auto& lines = wtl_it->second;
        lines.erase(find(lines, it->first));
        if (lines.empty())
            m_word_to_lines.erase(wtl_it);
    }
    return m_line_to_words.erase(it);
}

void WordDB::update_lines(LineToWords::iterator begin, LineToWords::iterator end,
                          LineCount num)
{
    std::vector<std::pair<LineCount, std::vector<String>>>
        to_update{std::make_move_iterator(begin), std::make_move_iterator(end)};
    m_line_to_words.erase(begin, end);

    for (auto& elem : to_update)
    {
        for (auto& word : elem.second)
        {
            auto& lines = m_word_to_lines[word];
            *find(lines, elem.first) += num;
        }
        elem.first += num;
    }
    m_line_to_words.insert(std::make_move_iterator(to_update.begin()),
                           std::make_move_iterator(to_update.end()));
}

void WordDB::on_insert(const Buffer& buffer, BufferCoord begin, BufferCoord end)
{
    auto num = end.line - begin.line;
    if (num > 0)
        update_lines(m_line_to_words.upper_bound(begin.line),
                     m_line_to_words.end(), num);

    remove_line(m_line_to_words.find(begin.line));
    for (auto line = begin.line; line <= end.line; ++line)
        add_words(line, buffer[line]);
}

void WordDB::on_erase(const Buffer& buffer, BufferCoord begin, BufferCoord end)
{
    auto first = m_line_to_words.lower_bound(begin.line);
    auto last = m_line_to_words.upper_bound(end.line);
    while (first != last)
        first = remove_line(first);

    auto num = end.line - begin.line;
    if (num > 0)
        update_lines(last, m_line_to_words.end(), -num);

    add_words(begin.line, buffer[begin.line]);
}

std::vector<String> WordDB::find_prefix(const String& prefix) const
{
    std::vector<String> res;
    for (auto it = m_word_to_lines.lower_bound(prefix); it != m_word_to_lines.end(); ++it)
    {
        if (not prefix_match(it->first, prefix))
            break;
        res.push_back(it->first);
    }
    return res;
}

}
