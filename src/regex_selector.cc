#include "regex_selector.hh"
#include "exception.hh"

namespace Kakoune
{

RegexSelector::RegexSelector(const std::string& exp)
    : m_regex(exp) {}

Selection RegexSelector::operator()(const BufferIterator& cursor) const
{
    BufferIterator begin = cursor;
    BufferIterator end = cursor;
    Selection::CaptureList captures;

    try
    {
        boost::match_results<BufferIterator> matches;

        if (boost::regex_search(cursor, cursor.buffer().end(), matches,
                                m_regex))
        {
            begin = matches[0].first;
            end   = matches[0].second;
            std::copy(matches.begin(), matches.end(),
                      std::back_inserter(captures));
        }
        else if (boost::regex_search(cursor.buffer().begin(), cursor, matches,
                                     m_regex))
        {
            begin = matches[0].first;
            end   = matches[0].second;
            std::copy(matches.begin(), matches.end(),
                      std::back_inserter(captures));
        }
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error("regex error");
    }


    if (begin == end)
        ++end;

    return Selection(begin, end - 1, std::move(captures));
}

SelectionList RegexSelector::operator()(const Selection& selection) const
{
    boost::regex_iterator<BufferIterator> re_it(selection.begin(),
                                                selection.end(),
                                                m_regex);
    boost::regex_iterator<BufferIterator> re_end;

    SelectionList result;
    for (; re_it != re_end; ++re_it)
    {
        BufferIterator begin = (*re_it)[0].first;
        BufferIterator end   = (*re_it)[0].second;

        if (begin == end)
           ++end;

        Selection::CaptureList captures(re_it->begin(), re_it->end());
        result.push_back(Selection(begin, end-1, std::move(captures)));
    }
    return result;
}

}
