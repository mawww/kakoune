#ifndef word_db_hh_INCLUDED
#define word_db_hh_INCLUDED

#include "buffer.hh"
#include "shared_string.hh"
#include "hash_map.hh"
#include "vector.hh"
#include "ranked_match.hh"

namespace Kakoune
{

using RankedMatchList = Vector<RankedMatch>;

// maintain a database of words available in a buffer
class WordDB : public OptionManagerWatcher
{
public:
    WordDB(const Buffer& buffer);
    ~WordDB() override;
    WordDB(const WordDB&) = delete;
    WordDB(WordDB&&);

    RankedMatchList find_matching(StringView str);

    int get_word_occurences(StringView word) const;
private:
    void update_db();
    void add_words(StringView line);
    void remove_words(StringView line);

    void rebuild_db();

    void on_option_changed(const Option& option) override;

    struct WordInfo
    {
        StringDataPtr word;
        UsedLetters letters;
        int refcount;
    };
    using WordToInfo = HashMap<StringView, WordInfo, MemoryDomain::WordDB>;
    using Lines = Vector<StringDataPtr, MemoryDomain::WordDB>;

    SafePtr<const Buffer> m_buffer;
    size_t m_timestamp;
    WordToInfo m_words;
    Lines m_lines;
};

}

#endif // word_db_hh_INCLUDED
