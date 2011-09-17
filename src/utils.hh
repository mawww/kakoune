#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

#include "exception.hh"

#include <memory>

namespace Kakoune
{

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

template<typename T, typename F>
class AutoRaii
{
public:
    AutoRaii(T* resource, F cleanup)
        : m_resource(resource), m_cleanup(cleanup) {}

    AutoRaii(AutoRaii&& other) : m_resource(other.m_resource),
                                 m_cleanup(other.m_cleanup)
    { other.m_resource = nullptr; }

    AutoRaii(const AutoRaii&) = delete;
    AutoRaii& operator=(const AutoRaii&) = delete;

    ~AutoRaii() { if (m_resource) m_cleanup(m_resource); }

    operator T*() { return m_resource; }

private:
    T* m_resource;
    F  m_cleanup;
};

template<typename T, typename F>
AutoRaii<T, F> auto_raii(T* resource, F cleanup)
{
    return AutoRaii<T, F>(resource, cleanup);
}

}

#endif // utils_hh_INCLUDED
