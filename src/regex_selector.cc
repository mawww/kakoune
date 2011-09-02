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
        while (not line_end.is_end() and *line_end != '\n')
            ++line_end;

        boost::match_results<BufferIterator> matches;

        if (boost::regex_search(cursor, line_end, matches, m_regex))
            return Selection(matches.begin()->first, matches.begin()->second);
    }
    catch (boost::regex_error& err)
    {
        print_status("regex error");
    }

    return Selection(cursor, cursor);
}

}
