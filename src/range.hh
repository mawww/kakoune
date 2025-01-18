#ifndef range_hh_INCLUDED
#define range_hh_INCLUDED

#include <cstddef>

namespace Kakoune
{

template<typename T>
struct Range
{
    T begin;
    T end;

    friend bool operator==(const Range& lhs, const Range& rhs) = default;

    friend size_t hash_value(const Range& range)
    {
        return hash_values(range.begin, range.end);
    }

    bool empty() const { return begin == end; }
};

}

#endif // range_hh_INCLUDED
