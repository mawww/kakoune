#ifndef utils_hh_INCLUDED
#define utils_hh_INCLUDED

#include "assert.hh"

#include <memory>

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
        kak_assert(ms_instance);
        return *static_cast<T*>(ms_instance);
    }

    static bool has_instance()
    {
        return ms_instance != nullptr;
    }

protected:
    Singleton()
    {
        kak_assert(ms_instance == nullptr);
        ms_instance = this;
    }

    ~Singleton()
    {
        kak_assert(ms_instance == this);
        ms_instance = nullptr;
    }

private:
    static Singleton* ms_instance;
};

template<typename T>
Singleton<T>* Singleton<T>::ms_instance = nullptr;

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
    [[gnu::always_inline]]
    OnScopeEnd(T func) : m_func{std::move(func)}, m_valid{true} {}

    [[gnu::always_inline]]
    OnScopeEnd(OnScopeEnd&& other)
      : m_func{std::move(other.m_func)}, m_valid{other.m_valid}
    { other.m_valid = false; }

    [[gnu::always_inline]]
    ~OnScopeEnd() noexcept(noexcept(std::declval<T>()())) { if (m_valid) m_func(); }

private:
    bool m_valid;
    T m_func;
};

template<typename T>
OnScopeEnd<T> on_scope_end(T t)
{
    return OnScopeEnd<T>{std::move(t)};
}

// bool that can be set (to true) multiple times, and will
// be false only when unset the same time;
struct NestedBool
{
    void set() { m_count++; }
    void unset() { kak_assert(m_count > 0); m_count--; }

    operator bool() const { return m_count > 0; }
private:
    int m_count = 0;
};

struct ScopedSetBool
{
    ScopedSetBool(NestedBool& nested_bool, bool condition = true)
        : m_nested_bool(nested_bool), m_condition(condition)
    {
        if (m_condition)
            m_nested_bool.set();
    }

    ~ScopedSetBool()
    {
        if (m_condition)
            m_nested_bool.unset();
    }

private:
    NestedBool& m_nested_bool;
    bool m_condition;
};

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

template<typename Iterator, typename EndIterator, typename T>
bool skip_while(Iterator& it, const EndIterator& end, T condition)
{
    while (it != end and condition(*it))
        ++it;
    return it != end;
}

template<typename Iterator, typename BeginIterator, typename T>
bool skip_while_reverse(Iterator& it, const BeginIterator& begin, T condition)
{
    while (it != begin and condition(*it))
        --it;
    return condition(*it);
}

template<typename E>
auto to_underlying(E value)
{
    return static_cast<std::underlying_type_t<E>>(value);
}

}

#endif // utils_hh_INCLUDED
