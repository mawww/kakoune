#include "filters.hh"

namespace Kakoune
{

void colorize_regex(DisplayBuffer& display_buffer,
                    const boost::regex& ex, Color color)
{
    for (auto atom_it = display_buffer.begin();
         atom_it != display_buffer.end(); ++atom_it)
    {
        boost::match_results<BufferIterator> matches;
        if (boost::regex_search(atom_it->begin, atom_it->end, matches, ex, boost::match_nosubs))
        {
            const BufferIterator& begin = matches.begin()->first;
            if (begin != atom_it->begin)
                atom_it = display_buffer.split(atom_it, begin) + 1;

            const BufferIterator& end = matches.begin()->second;
            if (end != atom_it->end)
                atom_it = display_buffer.split(atom_it, end);

            atom_it->fg_color = color;
        }
    }
}

void colorize_cplusplus(DisplayBuffer& display_buffer)
{
    static boost::regex preprocessor("(\\`|(?<=\\n))\\h*#\\h*[^\\n]*(?=\\n)");
    colorize_regex(display_buffer, preprocessor, Color::Magenta);

    static boost::regex comments("//[^\\n]*\\n");
    colorize_regex(display_buffer, comments, Color::Cyan);

    static boost::regex strings("(?<!')\"(\\\\\"|[^\"])*\"");
    colorize_regex(display_buffer, strings, Color::Magenta);

    static boost::regex values("\\<(true|false|NULL|nullptr)\\>|-?\\d+[fdiu]?|'\\\\?[^']?'");
    colorize_regex(display_buffer, values, Color::Red);

    static boost::regex builtin_types("\\<(void|int|float|bool|size_t)\\>");
    colorize_regex(display_buffer, builtin_types, Color::Yellow);

    static boost::regex control_keywords("\\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw)\\>");
    colorize_regex(display_buffer, control_keywords, Color::Blue);

    //static boost::regex operators("->|\\+|\\-|\\*|/|\\\\|\\&|\\|\\^|[<>=!+-]=|=|\\(|\\)|\\[|\\]|\\{|\\}|\\<(not|and|or|xor)\\>");
    //colorize_regex(display_buffer, operators, Color::Green);

    static boost::regex types_keywords("\\<(const|auto|namespace|static|volatile|class|struct|enum|union|public|protected|private|template)\\>");
    colorize_regex(display_buffer, types_keywords, Color::Green);
}

}
