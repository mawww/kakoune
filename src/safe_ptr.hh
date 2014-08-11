#ifndef safe_ptr_hh_INCLUDED
#define safe_ptr_hh_INCLUDED

// #define SAFE_PTR_TRACK_CALLSTACKS

#include "assert.hh"

#ifdef SAFE_PTR_TRACK_CALLSTACKS

#include <vector>
#include <algorithm>
#endif

namespace Kakoune
{

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
            m_ptr->inc_safe_count(this);
        #endif
    }
    safe_ptr(const safe_ptr& other) : safe_ptr(other.m_ptr) {}
    safe_ptr(safe_ptr&& other) : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
        #ifdef KAK_DEBUG
        if (m_ptr)
            m_ptr->safe_ptr_moved(&other, this);
        #endif
    }
    ~safe_ptr()
    {
        #ifdef KAK_DEBUG
        if (m_ptr)
            m_ptr->dec_safe_count(this);
        #endif
    }

    safe_ptr& operator=(const safe_ptr& other)
    {
        #ifdef KAK_DEBUG
        if (m_ptr != other.m_ptr)
        {
            if (m_ptr)
                m_ptr->dec_safe_count(this);
            if (other.m_ptr)
                other.m_ptr->inc_safe_count(this);
        }
        #endif
        m_ptr = other.m_ptr;
        return *this;
    }

    safe_ptr& operator=(safe_ptr&& other)
    {
        #ifdef KAK_DEBUG
        if (m_ptr)
            m_ptr->dec_safe_count(this);
        if (other.m_ptr)
            other.m_ptr->safe_ptr_moved(&other, this);
        #endif
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    void reset(T* ptr = nullptr)
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
    ~SafeCountable()
    {
        kak_assert(m_count == 0);
        #ifdef SAFE_PTR_TRACK_CALLSTACKS
        kak_assert(m_callstacks.empty());
        #endif
    }

    void inc_safe_count(void* ptr) const
    {
        ++m_count;
        #ifdef SAFE_PTR_TRACK_CALLSTACKS
        m_callstacks.emplace_back(ptr);
        #endif
    }
    void dec_safe_count(void* ptr) const
    {
        --m_count;
        kak_assert(m_count >= 0);
        #ifdef SAFE_PTR_TRACK_CALLSTACKS
        auto it = std::find_if(m_callstacks.begin(), m_callstacks.end(),
                               [=](const Callstack& cs) { return cs.ptr == ptr; });
        kak_assert(it != m_callstacks.end());
        m_callstacks.erase(it);
        #endif
    }

    void safe_ptr_moved(void* from, void* to) const
    {
        #ifdef SAFE_PTR_TRACK_CALLSTACKS
        auto it = std::find_if(m_callstacks.begin(), m_callstacks.end(),
                               [=](const Callstack& cs) { return cs.ptr == from; });
        kak_assert(it != m_callstacks.end());
        it->ptr = to;
        #endif
    }

private:
    #ifdef SAFE_PTR_TRACK_CALLSTACKS
    struct Backtrace
    {
        static constexpr int max_frames = 16;
        void* stackframes[max_frames];
        int num_frames = 0;

        Backtrace();
        const char* desc() const;
    };

    struct Callstack
    {
        Callstack(void* p) : ptr(p) {}
        void* ptr;
        Backtrace bt;
    };

    mutable std::vector<Callstack> m_callstacks;
    #endif
    mutable int m_count;
#endif
};

}

#endif // safe_ptr_hh_INCLUDED

