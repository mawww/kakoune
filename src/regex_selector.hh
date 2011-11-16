#ifndef regex_selector_hh_INCLUDED
#define regex_selector_hh_INCLUDED

#include "buffer.hh"
#include "window.hh"

#include <boost/regex.hpp>

namespace Kakoune
{

class RegexSelector
{
public:
    RegexSelector(const std::string& exp);

    Selection operator()(const BufferIterator& cursor) const;
    SelectionList operator()(const Selection& selection) const;

private:
    boost::regex m_regex;
};

}

#endif // regex_selector_hh_INCLUDED
