#include "display_buffer.hh"

namespace Kakoune
{

DisplayBuffer::DisplayBuffer()
{
}

LineAndColumn DisplayBuffer::dimensions() const
{
    return LineAndColumn();
}

}
