#include "filters.hh"

namespace Kakoune
{

void colorize_regex(DisplayBuffer& display_buffer,
                    const boost::regex& ex, Color color)
{
    for (auto atom_it = display_buffer.begin();
         atom_it != display_buffer.end(); ++atom_it)
    {
        boost::smatch matches;
        if (boost::regex_search(atom_it->content, matches, ex, boost::match_nosubs))
        {
            size_t pos = matches.begin()->first - atom_it->content.begin();
            if (pos != 0)
                atom_it = display_buffer.split(atom_it, pos) + 1;

            pos = matches.begin()->second - matches.begin()->first;
            if (pos != atom_it->content.length())
                atom_it = display_buffer.split(atom_it, pos);

            atom_it->fg_color = color;
        }
    }
}

}
