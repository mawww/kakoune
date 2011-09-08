#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

#include <memory>

namespace Kakoune
{

struct LineAndColumn
{
    int line;
    int column;

    LineAndColumn(int line = 0, int column = 0)
        : line(line), column(column) {}
};

template<typename Container>
struct ReversedContainer
{
    ReversedContainer(Container& container) : container(container) {}
    Container& container;

    decltype(container.rbegin()) begin() { return container.rbegin(); }
    decltype(container.rend())   end()   { return container.rend(); }
};

template<typename Container>
ReversedContainer<Container> reversed(Container& container)
{
    return ReversedContainer<Container>(container);
}

template<typename T>
bool operator== (const std::unique_ptr<T>& lhs, T* rhs)
{
    return lhs.get() == rhs;
}


}

#endif // utils_hh_INCLUDED
