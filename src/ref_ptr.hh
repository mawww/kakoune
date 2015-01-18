#ifndef ref_ptr_hh_INCLUDED
#define ref_ptr_hh_INCLUDED

namespace Kakoune
{

template<typename T>
struct ref_ptr
{
    ref_ptr() = default;
    ref_ptr(T* ptr) : m_ptr(ptr) { acquire(); }
    ~ref_ptr() { release(); }
    ref_ptr(const ref_ptr& other) : m_ptr(other.m_ptr) { acquire(); }
    ref_ptr(ref_ptr&& other) : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }

    ref_ptr& operator=(const ref_ptr& other)
    {
        release();
        m_ptr = other.m_ptr;
        acquire();
        return *this;
    }
    ref_ptr& operator=(ref_ptr&& other)
    {
        release();
        m_ptr = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }

    T* get() const { return m_ptr; }

    explicit operator bool() { return m_ptr; }

    friend bool operator==(const ref_ptr& lhs, const ref_ptr& rhs)
    {
        return lhs.m_ptr == rhs.m_ptr;
    }
    friend bool operator!=(const ref_ptr& lhs, const ref_ptr& rhs)
    {
        return lhs.m_ptr != rhs.m_ptr;
    }
private:
    T* m_ptr = nullptr;

    void acquire()
    {
        if (m_ptr)
            inc_ref_count(m_ptr);
    }

    void release()
    {
        if (m_ptr)
            dec_ref_count(m_ptr);
        m_ptr = nullptr;
    } 
};

}

#endif // ref_ptr_hh_INCLUDED
