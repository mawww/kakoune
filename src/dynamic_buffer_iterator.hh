#ifndef dynamic_buffer_iterator_hh_INCLUDED
#define dynamic_buffer_iterator_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

class DynamicBufferIterator : public BufferIterator,
                              public BufferModificationListener
{
public:
    DynamicBufferIterator() : BufferIterator() {}
    DynamicBufferIterator(const Buffer& buffer, BufferPos position);
    DynamicBufferIterator(const BufferIterator& other);
    DynamicBufferIterator(const DynamicBufferIterator& other)
        : BufferIterator(other) { register_ifp(); }

    DynamicBufferIterator(DynamicBufferIterator&& other);
    DynamicBufferIterator& operator=(const BufferIterator& other);
    DynamicBufferIterator& operator=(const DynamicBufferIterator& other)
    { return this->operator= (static_cast<const BufferIterator&>(other)); }
    ~DynamicBufferIterator();

   void on_modification(const BufferModification& modification);

private:
   void register_ifp();
   void unregister_ifn();
};


}

#endif // dynamic_buffer_iterator_hh_INCLUDED
