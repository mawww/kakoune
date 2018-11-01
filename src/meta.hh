#ifndef meta_hh_INCLUDED
#define meta_hh_INCLUDED

namespace Kakoune
{
inline namespace Meta
{

struct AnyType{};
template<typename T> struct Type : AnyType {};

template<typename T> using void_t = void;

}
}

#endif // meta_hh_INCLUDED
