#include "dynamic_buffer_iterator.hh"

namespace Kakoune
{

DynamicBufferIterator::DynamicBufferIterator(const Buffer& buffer,
                                             BufferPos position)
    : BufferIterator(buffer, position)
{
    register_ifp();
}

DynamicBufferIterator::DynamicBufferIterator(DynamicBufferIterator&& other)
    : BufferIterator(other)
{
    register_ifp();
}

DynamicBufferIterator::DynamicBufferIterator(const BufferIterator& other)
    : BufferIterator(other)
{
    register_ifp();
}

DynamicBufferIterator& DynamicBufferIterator::operator=(const BufferIterator& other)
{
    unregister_ifn();
    BufferIterator::operator=(other);
    register_ifp();

    return *this;
}

DynamicBufferIterator::~DynamicBufferIterator()
{
    unregister_ifn();
}

void DynamicBufferIterator::on_modification(const Modification& modification)
{
    if (*this < modification.position)
        return;

    size_t length = modification.content.length();
    if (modification.type == Modification::Erase)
    {
        // do not move length on the other side of the inequality,
        // as modification.position + length may be after buffer end
        if (*this - length <= modification.position)
            BufferIterator::operator=(modification.position);
        else
            *this -= length;
    }
    else
    {
        assert(modification.type == Modification::Insert);
        *this += length;
    }
}

void DynamicBufferIterator::register_ifp()
{
    if (is_valid())
        const_cast<Buffer&>(buffer()).register_modification_listener(this);
}

void DynamicBufferIterator::unregister_ifn()
{
    if (is_valid())
        const_cast<Buffer&>(buffer()).unregister_modification_listener(this);
}

}
