#include "regex_selector.hh"

void print_status(const std::string&);

namespace Kakoune
{

RegexSelector::RegexSelector(const std::string& exp)
    : m_regex(exp) {}

Selection RegexSelector::operator()(const BufferIterator& cursor) const
{
    BufferIterator line_end = cursor + 1;

    try
    {
        boost::match_results<BufferIterator> matches;

        if (boost::regex_search(cursor, cursor.buffer().end(), matches, m_regex, boost::match_nosubs))
            return Selection(matches.begin()->first, matches.begin()->second);
    }
    catch (boost::regex_error& err)
    {
        print_status("regex error");
    }

    return Selection(cursor, cursor);
}

}
