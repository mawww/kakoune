#include "regex_selector.hh"
#include "exception.hh"

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
        throw runtime_error("regex error");
    }

    return Selection(cursor, cursor);
}

}
