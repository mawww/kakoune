#ifndef word_db_hh_INCLUDED
#define word_db_hh_INCLUDED

#include "buffer.hh"
#include "interned_string.hh"

#include <map>
#include <bitset>

namespace Kakoune
{

// maintain a database of words available in a buffer
class WordDB
{
public:
    WordDB(const Buffer& buffer);
    WordDB(const WordDB&) { kak_assert(false); }
    WordDB(WordDB&&) = default;

    std::vector<InternedString> find_prefix(StringView prefix);
    std::vector<InternedString> find_subsequence(StringView subsequence);
    int get_word_occurences(StringView word) const;

    using UsedChars = std::bitset<64>;
    struct WordInfo
    {
        UsedChars letters;
        int refcount;
    };
    using WordList = UnorderedMap<InternedString, WordInfo>;
private:
    using LineToWords = std::vector<std::vector<InternedString>>;

    void update_db();

    safe_ptr<const Buffer> m_buffer;
    size_t m_timestamp;
    WordList m_words;
    LineToWords m_line_to_words;
};

}

#endif // word_db_hh_INCLUDED

