#include "display_buffer.hh"

#include "assert.hh"

namespace Kakoune
{

DisplayLine::iterator DisplayLine::split(iterator it, BufferIterator pos)
{
    assert(it->content.type() == AtomContent::BufferRange);
    assert(it->content.begin() < pos);
    assert(it->content.end() > pos);

    DisplayAtom atom = *it;
    atom.content.end() = pos;
    it->content.begin() = pos;
    return m_atoms.insert(it, std::move(atom));
}

}
