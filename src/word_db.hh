#ifndef word_db_hh_INCLUDED
#define word_db_hh_INCLUDED

#include "buffer.hh"
#include "line_change_watcher.hh"

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

    using WordList = std::map<String, int>;
private:
    using LineToWords = std::vector<std::vector<String>>;

    void update_db();

    LineChangeWatcher m_change_watcher;
    WordList m_words;
    LineToWords m_line_to_words;
};

}

#endif // word_db_hh_INCLUDED

