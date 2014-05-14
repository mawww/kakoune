#ifndef word_db_hh_INCLUDED
#define word_db_hh_INCLUDED

#include "buffer.hh"

#include <map>

namespace Kakoune
{

class String;

// maintain a database of words available in a buffer
class WordDB
{
public:
    WordDB(const Buffer& buffer);

    std::vector<String> find_prefix(const String& prefix);
    int get_word_occurences(const String& word) const;

    using WordList = std::map<String, int>;
private:
    using LineToWords = std::vector<std::vector<String>>;

    void update_db();

    safe_ptr<const Buffer> m_buffer;
    size_t m_timestamp;
    WordList m_words;
    LineToWords m_line_to_words;
};

}

#endif // word_db_hh_INCLUDED

