#ifndef meta_hh_INCLUDED
#define meta_hh_INCLUDED

namespace Kakoune::inline Meta
{

struct AnyType{};
template<typename T> struct Type : AnyType {};

}

#endif // meta_hh_INCLUDED
