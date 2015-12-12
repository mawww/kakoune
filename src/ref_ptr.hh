#ifndef ref_ptr_hh_INCLUDED
#define ref_ptr_hh_INCLUDED

#include <utility>

namespace Kakoune
{

struct WorstMatch { [[gnu::always_inline]] WorstMatch(...) {} };

[[gnu::always_inline]]
inline void ref_ptr_moved(WorstMatch, void*, void*) noexcept {}

template<typename T, typename TForOverload = T>
struct RefPtr
{
    RefPtr() = default;
    explicit RefPtr(T* ptr) : m_ptr(ptr) { acquire(); }
    ~RefPtr() { release(); }
    RefPtr(const RefPtr& other) : m_ptr(other.m_ptr) { acquire(); }
    RefPtr(RefPtr&& other)
        noexcept(noexcept(std::declval<RefPtr>().moved(nullptr)))
        : m_ptr(other.m_ptr) { other.m_ptr = nullptr; moved(&other); }

    RefPtr& operator=(const RefPtr& other)
    {
        if (other.m_ptr != m_ptr)
        {
            release();
            m_ptr = other.m_ptr;
            acquire();
        }
        return *this;
    }

    RefPtr& operator=(RefPtr&& other)
    {
        release();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        moved(&other);
        return *this;
    }

    RefPtr& operator=(T* ptr)
    {
        if (ptr != m_ptr)
        {
            release();
            m_ptr = ptr;
            acquire();
        }
        return *this;
    }

    [[gnu::always_inline]]
    T* operator->() const { return m_ptr; }
    [[gnu::always_inline]]
    T& operator*() const { return *m_ptr; }

    [[gnu::always_inline]]
    T* get() const { return m_ptr; }

    [[gnu::always_inline]]
    explicit operator bool() const { return m_ptr; }

    void reset(T* ptr = nullptr)
    {
        if (ptr == m_ptr)
            return;
        release();
        m_ptr = ptr;
        acquire();
    }

    friend bool operator==(const RefPtr& lhs, const RefPtr& rhs) { return lhs.m_ptr == rhs.m_ptr; }
    friend bool operator!=(const RefPtr& lhs, const RefPtr& rhs) { return lhs.m_ptr != rhs.m_ptr; }

private:
    T* m_ptr = nullptr;

    [[gnu::always_inline]]
    void acquire()
    {
        if (m_ptr)
            inc_ref_count(static_cast<TForOverload*>(m_ptr), this);
    }

    [[gnu::always_inline]]
    void release()
    {
        if (m_ptr)
            dec_ref_count(static_cast<TForOverload*>(m_ptr), this);
        m_ptr = nullptr;
    }

    [[gnu::always_inline]]
    void moved(void* from)
        noexcept(noexcept(ref_ptr_moved(static_cast<TForOverload*>(nullptr), nullptr, nullptr)))
    {
        if (m_ptr)
            ref_ptr_moved(static_cast<TForOverload*>(m_ptr), from, this);
    }
};

struct RefCountable
{
    int refcount = 0;
    virtual ~RefCountable() = default;

    friend void inc_ref_count(RefCountable* r, void*)
    {
        ++r->refcount;
    }

    friend void dec_ref_count(RefCountable* r, void*)
    {
        if (--r->refcount == 0)
            delete r;
    }
};

}

#endif // ref_ptr_hh_INCLUDED
