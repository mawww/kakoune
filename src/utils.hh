#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

#include "assert.hh"

#include <memory>

namespace Kakoune
{

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<size_t... I>
struct IndexSequence
{
    using Next = IndexSequence<I..., sizeof...(I)>;
};

template<size_t N>
struct MakeIndexSequence
{
    using Type = typename MakeIndexSequence<N-1>::Type::Next;
};

template<>
struct MakeIndexSequence<0>
{
    using Type = IndexSequence<>;
};

template<size_t N>
constexpr typename MakeIndexSequence<N>::Type
make_index_sequence() { return typename MakeIndexSequence<N>::Type{}; }

// *** Singleton ***
//
// Singleton helper class, every singleton type T should inherit
// from Singleton<T> to provide a consistent interface.
template<typename T>
class Singleton
{
public:
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

    static T& instance()
    {
        kak_assert (ms_instance);
        return *ms_instance;
    }

    static bool has_instance()
    {
        return ms_instance != nullptr;
    }

protected:
    Singleton()
    {
        kak_assert(not ms_instance);
        ms_instance = static_cast<T*>(this);
    }

    ~Singleton()
    {
        kak_assert(ms_instance == this);
        ms_instance = nullptr;
    }

private:
    static T* ms_instance;
};

template<typename T>
T* Singleton<T>::ms_instance = nullptr;

template<typename Iterator, typename EndIterator, typename T>
void skip_while(Iterator& it, const EndIterator& end, T condition)
{
    while (it != end and condition(*it))
        ++it;
}

template<typename Iterator, typename BeginIterator, typename T>
void skip_while_reverse(Iterator& it, const BeginIterator& begin, T condition)
{
    while (it != begin and condition(*it))
        --it;
}

// *** On scope end ***
//
// on_scope_end provides a way to register some code to be
// executed when current scope closes.
//
// usage:
// auto cleaner = on_scope_end([]() { ... });
//
// This permits to cleanup c-style resources without implementing
// a wrapping class
template<typename T>
class OnScopeEnd
{
public:
    OnScopeEnd(T func) : m_func(std::move(func)) {}
    ~OnScopeEnd() { m_func(); }
private:
    T m_func;
};

template<typename T>
OnScopeEnd<T> on_scope_end(T t)
{
    return OnScopeEnd<T>(t);
}

// *** Misc helper functions ***

template<typename T>
bool operator== (const std::unique_ptr<T>& lhs, T* rhs)
{
    return lhs.get() == rhs;
}

template<typename T>
const T& clamp(const T& val, const T& min, const T& max)
{
    return (val < min ? min : (val > max ? max : val));
}

}

#endif // utils_hh_INCLUDED
