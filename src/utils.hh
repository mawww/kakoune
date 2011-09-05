#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

namespace Kakoune
{

struct LineAndColumn
{
    int line;
    int column;

    LineAndColumn(int line = 0, int column = 0)
        : line(line), column(column) {}
};

}

#endif // utils_hh_INCLUDED
