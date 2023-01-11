#include "string.hh"

#include <cstdio>
#include <cstring>

namespace Kakoune
{

String::Data::Data(const char* data, size_t size, size_t capacity)
{
    if (capacity > Short::capacity)
    {
        if (capacity & 1)
            ++capacity;

        kak_assert(capacity < Long::max_capacity);
        u.l.ptr = Alloc{}.allocate(capacity+1);
        u.l.size = size;
        u.l.capacity = capacity;

        if (data != nullptr)
            memcpy(u.l.ptr, data, size);
        u.l.ptr[size] = 0;
    }
    else
        set_short(data, size);
}

String::Data& String::Data::operator=(const Data& other)
{
    if (&other == this)
        return *this;

    const size_t new_size = other.size();
    reserve<false>(new_size);
    memcpy(data(), other.data(), new_size+1);
    set_size(new_size);

    return *this;
}

String::Data& String::Data::operator=(Data&& other) noexcept
{
    if (&other == this)
        return *this;

    release();

    if (other.is_long())
    {
        u.l = other.u.l;
        other.set_empty();
    }
    else
        u.s = other.u.s;

    return *this;
}

template<bool copy>
void String::Data::reserve(size_t new_capacity)
{
    if (capacity() != 0 and new_capacity <= capacity())
        return;

    if (is_long())
        new_capacity = std::max(u.l.capacity * 2, new_capacity);

    if (new_capacity & 1)
        ++new_capacity;

    kak_assert(new_capacity < Long::max_capacity);
    char* new_ptr = Alloc{}.allocate(new_capacity+1);
    if (copy)
    {
        memcpy(new_ptr, data(), size()+1);
        u.l.size = size();
    }
    release();

    u.l.ptr = new_ptr;
    u.l.capacity = new_capacity;
}

template void String::Data::reserve<true>(size_t);
template void String::Data::reserve<false>(size_t);

void String::Data::force_size(size_t new_size)
{
    reserve<false>(new_size);
    set_size(new_size);
}

void String::Data::append(const char* str, size_t len)
{
    if (len == 0)
        return;

    const size_t new_size = size() + len;
    reserve(new_size);

    memcpy(data() + size(), str, len);
    set_size(new_size);
    data()[new_size] = 0;
}

void String::Data::clear()
{
    release();
    set_empty();
}

void String::resize(ByteCount size, char c)
{
    const size_t target_size = (size_t)size;
    const size_t current_size = m_data.size();
    if (target_size < current_size)
        m_data.set_size(target_size);
    else if (target_size > current_size)
    {
        m_data.reserve(target_size);
        m_data.set_size(target_size);
        for (auto i = current_size; i < target_size; ++i)
            m_data.data()[i] = c;
    }
    data()[target_size] = 0;
}

void String::Data::set_size(size_t size)
{
    if (is_long())
        u.l.size = size;
    else
        u.s.size = (size << 1) | 1;
}

void String::Data::set_short(const char* data, size_t size)
{
    u.s.size = (size << 1) | 1;
    if (data != nullptr)
        memcpy(u.s.string, data, size);
    u.s.string[size] = 0;
}

const String String::ms_empty;

}
