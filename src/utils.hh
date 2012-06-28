#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

#include "exception.hh"
#include "assert.hh"

#include <memory>
#include <algorithm>

namespace Kakoune
{
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
        assert (ms_instance);
        return *ms_instance;
    }

    static void delete_instance()
    {
        delete ms_instance;
        ms_instance = nullptr;
    }

protected:
    Singleton()
    {
        assert(not ms_instance);
        ms_instance = static_cast<T*>(this);
    }

    ~Singleton()
    {
        assert(ms_instance == this);
        ms_instance = nullptr;
    }

private:
    static T* ms_instance;
};

template<typename T>
T* Singleton<T>::ms_instance = nullptr;

// *** safe_ptr: objects that assert nobody references them when they die ***

typedef void* (*unspecified_bool_type)();

template<typename T>
class safe_ptr
{
public:
    safe_ptr() : m_ptr(nullptr) {}
    explicit safe_ptr(T* ptr) : m_ptr(ptr) { if (ptr) ptr->inc_safe_count(); }
    safe_ptr(const safe_ptr& other) : safe_ptr(other.m_ptr) {}
    safe_ptr(safe_ptr&& other) : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    ~safe_ptr() { if (m_ptr) m_ptr->dec_safe_count(); }

    safe_ptr& operator=(const safe_ptr& other)
    {
        if (m_ptr != other.m_ptr)
        {
            if (m_ptr)
                m_ptr->dec_safe_count();
            m_ptr = other.m_ptr;
            if (m_ptr)
                m_ptr->inc_safe_count();
        }

        return *this;
   }

   bool operator== (const safe_ptr& other) const { return m_ptr == other.m_ptr; }
   bool operator!= (const safe_ptr& other) const { return m_ptr != other.m_ptr; }
   bool operator== (T* ptr) const { return m_ptr == ptr; }
   bool operator!= (T* ptr) const { return m_ptr != ptr; }

   T& operator*  () const { return *m_ptr; }
   T* operator-> () const { return m_ptr; }

   T* get() const { return m_ptr; }

   operator unspecified_bool_type() const { return (unspecified_bool_type)(m_ptr); }

private:
   T* m_ptr;
};

class SafeCountable
{
public:
   SafeCountable() : m_count(0) {}
   ~SafeCountable() { assert(m_count == 0); }

   void inc_safe_count() { ++m_count; }
   void dec_safe_count() { --m_count; assert(m_count >= 0); }

private:
   int m_count;
};

// *** Containers helpers ***

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


template<typename Container, typename T>
auto find(Container& container, const T& value) -> decltype(container.begin())
{
    return std::find(container.begin(), container.end(), value);
}

template<typename Container, typename T>
bool contains(const Container& container, const T& value)
{
    return find(container, value) != container.end();
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
    OnScopeEnd(T func) : m_func(func) {}
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

}

#endif // utils_hh_INCLUDED
