#include "string.hh"

#include <cstring>
#include "assert.hh"
#include "unit_tests.hh"

namespace Kakoune
{
namespace
{
// Avoid including all of <algorithm> just for this.
constexpr auto max(auto lhs, auto rhs) { return lhs > rhs ? lhs : rhs;}
constexpr auto min(auto lhs, auto rhs) { return lhs < rhs ? lhs : rhs;}
}

String::Data::Data(const char* data, size_t size, size_t capacity)
{
    if (capacity > Short::capacity)
    {
        kak_assert(capacity <= Long::max_capacity);
        u.l.ptr = Alloc{}.allocate(capacity+1);
        u.l.size = size;
        u.l.capacity = (capacity & Long::max_capacity);
        u.l.mode = Long::active_mask;

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
    auto const current_capacity = capacity();
    if (current_capacity != 0 and new_capacity <= current_capacity)
        return;

    if (!is_long() and new_capacity <= Short::capacity)
        return;

    kak_assert(new_capacity <= Long::max_capacity);
    new_capacity = max(new_capacity, // Do not upgrade new_capacity to be over limit.
                       min(current_capacity * 2, Long::max_capacity));

    char* new_ptr = Alloc{}.allocate(new_capacity+1);
    if (copy)
    {
        memcpy(new_ptr, data(), size()+1);
    }
    release();

    u.l.size = size();
    u.l.ptr = new_ptr;
    u.l.capacity = (new_capacity & Long::max_capacity);
    u.l.mode = Long::active_mask;
}

template void String::Data::reserve<true>(size_t);
template void String::Data::reserve<false>(size_t);

void String::Data::force_size(size_t new_size)
{
    reserve<false>(new_size);
    set_size(new_size);
    data()[new_size] = 0;
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

String::String(Codepoint cp, CharCount count)
{
    reserve(utf8::codepoint_size(cp) * (int)count);
    while (count-- > 0)
        utf8::dump(std::back_inserter(*this), cp);
}

String::String(Codepoint cp, ColumnCount count)
{
    int cp_count = (int)(count / max(codepoint_width(cp), 1_col));
    reserve(utf8::codepoint_size(cp) * cp_count);
    while (cp_count-- > 0)
        utf8::dump(std::back_inserter(*this), cp);
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
        u.s.remaining_size = Short::capacity - size;
}

void String::Data::set_short(const char* data, size_t size)
{
    kak_assert(size <= Short::capacity);
    u.s.remaining_size = Short::capacity - size;
    if (data != nullptr)
        memcpy(u.s.string, data, size);
    if (size != Short::capacity) // in this case, remaining_size is the null terminator
        u.s.string[size] = 0;
}

UnitTest test_data{[]{
  using Data = String::Data;
    { // Basic data usage.
        Data data;
        kak_assert(data.size() == 0);
        kak_assert(not data.is_long());
        kak_assert(data.capacity() == 23);

        // Should be SSO-ed.
        data.append("test", 4);
        kak_assert(data.size() == 4);
        kak_assert(data.capacity() == 23);
        kak_assert(not data.is_long());
        kak_assert(data.data() == StringView("test"));
    }
    {
        char large_buf[2048];
        memset(large_buf, 'x', 2048);
        Data data(large_buf, 2048);
        kak_assert(data.size() == 2048);
        kak_assert(data.capacity() >= 2048);

        data.clear();
        kak_assert(data.size() == 0);
        kak_assert(not data.is_long());
        kak_assert(data.capacity() == 23);
    }
}};

const String String::ms_empty;

}
