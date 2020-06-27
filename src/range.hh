#ifndef range_hh_INCLUDED
#define range_hh_INCLUDED

namespace Kakoune
{

template<typename T>
struct Range
{
    T begin;
    T end;

    friend bool operator==(const Range& lhs, const Range& rhs)
    {
        return lhs.begin == rhs.begin and lhs.end == rhs.end;
    }

    friend bool operator!=(const Range& lhs, const Range& rhs)
    {
        return not (lhs == rhs);
    }

    friend size_t hash_value(const Range& range)
    {
        return hash_values(range.begin, range.end);
    }

    bool empty() const { return begin == end; }
};

}

#endif // range_hh_INCLUDED
