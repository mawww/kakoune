#ifndef modification_hh_INCLUDED
#define modification_hh_INCLUDED

#include "coord.hh"
#include "utils.hh"
#include "buffer.hh"

namespace Kakoune
{

struct Modification
{
    ByteCoord old_coord;
    ByteCoord new_coord;
    ByteCoord num_removed;
    ByteCoord num_added;

    ByteCoord added_end() const
    {
        if (num_added.line)
            return { new_coord.line + num_added.line, num_added.column };
        else
            return { new_coord.line, new_coord.column + num_added.column };
    }

    ByteCoord get_old_coord(ByteCoord coord) const
    {
        if (coord.line == new_coord.line)
        {
            if (num_added.line == 0)
                coord.column -= new_coord.column - old_coord.column + num_added.column - num_removed.column;
            else
                coord.column -= num_added.column - num_removed.column;
        }
        coord.line -= new_coord.line - old_coord.line + num_added.line - num_removed.line;
        return coord;
    }

    ByteCoord get_new_coord(ByteCoord coord, bool& deleted) const
    {
        deleted = false;
        if (coord < old_coord)
            return coord;

        // apply remove
        if (coord.line < old_coord.line + num_removed.line or
            (coord.line == old_coord.line + num_removed.line and
             coord.column < old_coord.column + num_removed.column))
        {
            deleted = true;
            coord = old_coord;
        }
        else if (coord.line == old_coord.line + num_removed.line)
        {
            coord.line = old_coord.line;
            coord.column -= num_removed.column;
        }

        // apply move
        coord.line += new_coord.line - old_coord.line;
        coord.column += new_coord.column - old_coord.column;

        // apply add
        if (coord.line == new_coord.line)
        {
            if (num_added.line == 0)
                coord.column += num_added.column;
            else
                coord.column += num_added.column - new_coord.column;
        }
        coord.line += num_added.line;

        return coord;
    }

    ByteCoord get_new_coord(ByteCoord coord) const
    {
        bool dummy;
        return get_new_coord(coord, dummy);
    }
};

std::vector<Modification> compute_modifications(const Buffer& buffer, size_t timestamp);
std::vector<Modification> compute_modifications(memoryview<Buffer::Change> changes);

}

#endif // modification_hh_INCLUDED
