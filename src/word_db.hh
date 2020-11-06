#ifndef word_db_hh_INCLUDED
#define word_db_hh_INCLUDED

#include "shared_string.hh"
#include "hash_map.hh"
#include "vector.hh"
#include "ranked_match.hh"
#include "option_manager.hh"
#include "safe_ptr.hh"

namespace Kakoune
{

using RankedMatchList = Vector<RankedMatch>;
class Buffer;

// maintain a database of words available in a buffer
class WordDB : public OptionManagerWatcher
{
public:
    static constexpr CharCount max_word_len = 50;

    WordDB(const Buffer& buffer);
    ~WordDB();
    WordDB(const WordDB&) = delete;
    WordDB(WordDB&&) noexcept;

    RankedMatchList find_matching(StringView str);

    int get_word_occurences(StringView word) const;
private:
    void update_db();
    void add_words(StringView line, ConstArrayView<Codepoint> extra_word_chars);
    void remove_words(StringView line, ConstArrayView<Codepoint> extra_word_chars);

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

WordDB& get_word_db(const Buffer& buffer);

}

#endif // word_db_hh_INCLUDED
