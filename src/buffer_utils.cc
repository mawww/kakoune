#include "buffer_utils.hh"

namespace Kakoune
{

CharCount get_column(const Buffer& buffer,
                     CharCount tabstop, BufferCoord coord)
{
    auto& line = buffer[coord.line];
    auto col = 0_char;
    for (auto it = line.begin();
         it != line.end() and coord.column > (int)(it - line.begin());
         it = utf8::next(it))
    {
        if (*it == '\t')
            col = (col / tabstop + 1) * tabstop;
        else
            ++col;
    }
    return col;
}

}
