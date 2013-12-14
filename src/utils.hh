#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

#include "assert.hh"
#include "exception.hh"

#include <algorithm>
#include <memory>
#include <vector>
#include <unordered_set>

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
        kak_assert (ms_instance);
        return *ms_instance;
    }

    static void delete_instance()
    {
        delete ms_instance;
        ms_instance = nullptr;
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

// *** safe_ptr: objects that assert nobody references them when they die ***

template<typename T>
class safe_ptr
{
public:
    safe_ptr() : m_ptr(nullptr) {}
    explicit safe_ptr(T* ptr) : m_ptr(ptr)
    {
        #ifdef KAK_DEBUG
        if (m_ptr)
            m_ptr->inc_safe_count();
        #endif
    }
    safe_ptr(const safe_ptr& other) : safe_ptr(other.m_ptr) {}
    safe_ptr(safe_ptr&& other) : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }
    ~safe_ptr()
    {
        #ifdef KAK_DEBUG
        if (m_ptr)
            m_ptr->dec_safe_count();
        #endif
    }

    safe_ptr& operator=(const safe_ptr& other)
    {
        #ifdef KAK_DEBUG
        if (m_ptr != other.m_ptr)
        {
            if (m_ptr)
                m_ptr->dec_safe_count();
            if (other.m_ptr)
                other.m_ptr->inc_safe_count();
        }
        #endif
        m_ptr = other.m_ptr;
        return *this;
    }

    safe_ptr& operator=(safe_ptr&& other)
    {
        #ifdef KAK_DEBUG
        if (m_ptr)
            m_ptr->dec_safe_count();
        #endif
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    void reset(T* ptr)
    {
        *this = safe_ptr(ptr);
    }

    bool operator== (const safe_ptr& other) const { return m_ptr == other.m_ptr; }
    bool operator!= (const safe_ptr& other) const { return m_ptr != other.m_ptr; }
    bool operator== (T* ptr) const { return m_ptr == ptr; }
    bool operator!= (T* ptr) const { return m_ptr != ptr; }

    T& operator*  () const { return *m_ptr; }
    T* operator-> () const { return m_ptr; }

    T* get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr; }

private:
    T* m_ptr;
};

class SafeCountable
{
public:
#ifdef KAK_DEBUG
    SafeCountable() : m_count(0) {}
    ~SafeCountable() { kak_assert(m_count == 0); }

    void inc_safe_count() const { ++m_count; }
    void dec_safe_count() const { --m_count; kak_assert(m_count >= 0); }

private:
    mutable int m_count;
#endif
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
ReversedContainer<Container> reversed(Container&& container)
{
    return ReversedContainer<Container>(container);
}


template<typename Container, typename T>
auto find(Container&& container, const T& value) -> decltype(container.begin())
{
    return std::find(container.begin(), container.end(), value);
}

template<typename Container, typename T>
auto find_if(Container&& container, T op) -> decltype(container.begin())
{
    return std::find_if(container.begin(), container.end(), op);
}


template<typename Container, typename T>
bool contains(Container&& container, const T& value)
{
    return find(container, value) != container.end();
}

template<typename T1, typename T2>
bool contains(const std::unordered_set<T1>& container, const T2& value)
{
    return container.find(value) != container.end();
}

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

inline String escape(const String& name)
{
    static Regex ex{"([ \\t;])"};
    return boost::regex_replace(name, ex, R"(\\\1)");
}

template<typename T>
const T& clamp(const T& val, const T& min, const T& max)
{
    return (val < min ? min : (val > max ? max : val));
}

template<typename T>
bool is_in_range(const T& val, const T& min, const T& max)
{
    return min <= val and val <= max;
}

// *** AutoRegister: RAII handling of value semantics registering classes ***

template<typename EffectiveType, typename RegisterFuncs, typename Registry>
class AutoRegister
{
public:
    AutoRegister(Registry& registry)
        : m_registry(&registry)
    {
        RegisterFuncs::insert(*m_registry, effective_this());
    }

    AutoRegister(const AutoRegister& other)
        : m_registry(other.m_registry)
    {
        RegisterFuncs::insert(*m_registry, effective_this());
    }

    AutoRegister(AutoRegister&& other)
        : m_registry(other.m_registry)
    {
        RegisterFuncs::insert(*m_registry, effective_this());
    }

    ~AutoRegister()
    {
        RegisterFuncs::remove(*m_registry, effective_this());
    }

    AutoRegister& operator=(const AutoRegister& other)
    {
        if (m_registry != other.m_registry)
        {
            RegisterFuncs::remove(*m_registry, effective_this());
            m_registry = other.m_registry;
            RegisterFuncs::insert(*m_registry, effective_this());
        }
        return *this;
    }

    AutoRegister& operator=(AutoRegister&& other)
    {
        if (m_registry != other.m_registry)
        {
            RegisterFuncs::remove(*m_registry, effective_this());
            m_registry = other.m_registry;
            RegisterFuncs::insert(*m_registry, effective_this());
        }
        return *this;
    }
    Registry& registry() const { return *m_registry; }

private:
    EffectiveType& effective_this() { return static_cast<EffectiveType&>(*this); }
    Registry* m_registry;
};

}

// std::pair hashing
namespace std
{

template<typename T1, typename T2>
struct hash<std::pair<T1,T2>>
{
    size_t operator()(const std::pair<T1,T2>& val) const
    {
        size_t seed = std::hash<T2>()(val.second);
        return seed ^ (std::hash<T1>()(val.first) + 0x9e3779b9 +
                       (seed << 6) + (seed >> 2));
    }
};

}

#endif // utils_hh_INCLUDED
