#ifndef ref_ptr_hh_INCLUDED
#define ref_ptr_hh_INCLUDED

#include <utility>

namespace Kakoune
{

struct RefCountable
{
    RefCountable() = default;
    RefCountable(const RefCountable&) {}
    RefCountable(RefCountable&&) {}
    virtual ~RefCountable() = default;

    RefCountable& operator=(const RefCountable&) { return *this; }
    RefCountable& operator=(RefCountable&&) { return *this; }

    int refcount = 0;
};

struct RefCountablePolicy
{
    static void inc_ref(RefCountable* r, void*) noexcept { ++r->refcount; }
    static void dec_ref(RefCountable* r, void*) noexcept { if (--r->refcount == 0) delete r; }
    static void ptr_moved(RefCountable*, void*, void*) noexcept {}
};

template<typename T, typename Policy = RefCountablePolicy>
struct RefPtr
{
    RefPtr() = default;
    explicit RefPtr(T* ptr) : m_ptr(ptr) { acquire(); }
    ~RefPtr() noexcept { release(); }
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

    friend bool operator==(const RefPtr& lhs, const RefPtr& rhs) = default;
    friend bool operator==(const RefPtr& lhs, const T* rhs) { return lhs.m_ptr == rhs; }

private:
    T* m_ptr = nullptr;

    [[gnu::always_inline]]
    void acquire()
    {
        if (m_ptr)
            Policy::inc_ref(m_ptr, this);
    }

    [[gnu::always_inline]]
    void release() noexcept
    {
        if (m_ptr)
            Policy::dec_ref(m_ptr, this);
    }

    [[gnu::always_inline]]
    void moved(void* from)
        noexcept(noexcept(Policy::ptr_moved(nullptr, nullptr, nullptr)))
    {
        if (m_ptr)
            Policy::ptr_moved(m_ptr, from, this);
    }
};

}

#endif // ref_ptr_hh_INCLUDED
