#include "regex_selector.hh"
#include "exception.hh"

namespace Kakoune
{

RegexSelector::RegexSelector(const std::string& exp)
    : m_regex(exp) {}

Selection RegexSelector::operator()(const BufferIterator& cursor) const
{
    try
    {
        boost::match_results<BufferIterator> matches;

        if (boost::regex_search(cursor, cursor.buffer().end(), matches, m_regex, boost::match_nosubs))
            return Selection(matches.begin()->first, matches.begin()->second-1);
        else if (boost::regex_search(cursor.buffer().begin(), cursor, matches, m_regex, boost::match_nosubs))
            return Selection(matches.begin()->first, matches.begin()->second-1);
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error("regex error");
    }

    return Selection(cursor, cursor);
}

SelectionList RegexSelector::operator()(const Selection& selection) const
{
    boost::regex_iterator<BufferIterator> re_it(selection.begin(),
                                                selection.end(),
                                                m_regex, boost::match_nosubs);
    boost::regex_iterator<BufferIterator> re_end;

    SelectionList result;
    for (; re_it != re_end; ++re_it)
    {
        BufferIterator begin = (*re_it)[0].first;
        BufferIterator end   = (*re_it)[0].second;
        assert(begin != end);

        result.push_back(Selection(begin, end-1));
    }
    return result;
}

}
