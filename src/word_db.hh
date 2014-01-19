#ifndef word_db_hh_INCLUDED
#define word_db_hh_INCLUDED

#include "buffer.hh"

#include <set>

namespace Kakoune
{

class String;

// maintain a database of words available in a buffer
class WordDB : public BufferChangeListener_AutoRegister
{
public:
    WordDB(const Buffer& buffer);

    void on_insert(const Buffer& buffer, BufferCoord begin, BufferCoord end) override;
    void on_erase(const Buffer& buffer, BufferCoord begin, BufferCoord end) override;

    std::vector<String> find_prefix(const String& prefix) const;

private:
    using WordToLines = std::map<String, std::vector<LineCount>>;
    using LineToWords = std::map<LineCount, std::vector<String>>;

    void add_words(LineCount line, const String& content);
    LineToWords::iterator remove_line(LineToWords::iterator it);
    void update_lines(LineToWords::iterator begin, LineToWords::iterator end,
                      LineCount num);

    WordToLines m_word_to_lines;
    LineToWords m_line_to_words;
};

}

#endif // word_db_hh_INCLUDED

