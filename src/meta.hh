#pragma once

namespace Kakoune
{
inline namespace Meta
{

struct AnyType{};
template<typename T> struct Type : AnyType {};

}
}
