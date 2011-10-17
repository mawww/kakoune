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
        if (boost::regex_search(atom_it->begin(), atom_it->end(),
                                matches, ex, boost::match_nosubs))
        {
            const BufferIterator& begin = matches.begin()->first;
            if (begin != atom_it->begin())
                atom_it = display_buffer.split(atom_it, begin) + 1;

            const BufferIterator& end = matches.begin()->second;
            if (end != atom_it->end())
                atom_it = display_buffer.split(atom_it, end);

            atom_it->fg_color() = color;
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

void expand_tabulations(DisplayBuffer& display_buffer)
{
    const int tabstop = 8;
    for (auto atom_it = display_buffer.begin();
         atom_it != display_buffer.end(); ++atom_it)
    {
        for (BufferIterator it = atom_it->begin(); it != atom_it->end(); ++it)
        {
            if (*it == '\t')
            {
                if (it != atom_it->begin())
                    atom_it = display_buffer.split(atom_it, it) + 1;

                if (it+1 != atom_it->end())
                    atom_it = display_buffer.split(atom_it, it+1);

                BufferCoord pos = it.buffer().line_and_column_at(it);

                int column = 0;
                for (auto line_it = it.buffer().iterator_at({pos.line, 0});
                     line_it != it; ++line_it)
                {
                    assert(*line_it != '\n');
                    if (*line_it == '\t')
                        column += tabstop - (column % tabstop);
                    else
                       ++column;
                }

                int count = tabstop - (column % tabstop);
                display_buffer.replace_atom_content(atom_it,
                                                    std::string(count, ' '));
            }
        }
    }
}

void show_line_numbers(DisplayBuffer& display_buffer)
{
    const Buffer& buffer = display_buffer.front().begin().buffer();
    BufferCoord coord = buffer.line_and_column_at(display_buffer.begin()->begin());

    int last_line = buffer.line_and_column_at(display_buffer.back().end()-1).line;

    for (; coord.line <= last_line; ++coord.line)
    {
        BufferIterator line_start = buffer.iterator_at(coord);
        DisplayBuffer::iterator atom_it = display_buffer.atom_containing(line_start);
        if (atom_it != display_buffer.end())
        {
            if (atom_it->begin() != line_start)
            {
                if (not atom_it->splitable())
                    continue;

                atom_it = display_buffer.split(atom_it, line_start) + 1;
            }
            atom_it = display_buffer.insert(
                atom_it,
                DisplayAtom(atom_it->coord(),
                            atom_it->begin(), atom_it->begin(),
                            Color::Black, Color::White));

            char buffer[6];
            snprintf(buffer, 6, "%3d ", coord.line);
            display_buffer.replace_atom_content(atom_it, buffer);
        }
    }
}

}
