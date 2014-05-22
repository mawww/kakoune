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

    ByteCoord added_end() const;
    ByteCoord removed_end() const;

    ByteCoord get_old_coord(ByteCoord coord) const;
    ByteCoord get_new_coord(ByteCoord coord) const;
};

std::vector<Modification> compute_modifications(const Buffer& buffer, size_t timestamp);
std::vector<Modification> compute_modifications(memoryview<Buffer::Change> changes);

ByteCoord update_pos(memoryview<Modification> modifs, ByteCoord pos);


}

#endif // modification_hh_INCLUDED
