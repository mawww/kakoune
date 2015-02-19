#ifndef ref_ptr_hh_INCLUDED
#define ref_ptr_hh_INCLUDED

namespace Kakoune
{

template<typename T>
struct RefPtr
{
    RefPtr() = default;
    RefPtr(T* ptr) : m_ptr(ptr) { acquire(); }
    ~RefPtr() { release(); }
    RefPtr(const RefPtr& other) : m_ptr(other.m_ptr) { acquire(); }
    RefPtr(RefPtr&& other) : m_ptr(other.m_ptr) { other.m_ptr = nullptr; }

    RefPtr& operator=(const RefPtr& other)
    {
        release();
        m_ptr = other.m_ptr;
        acquire();
        return *this;
    }
    RefPtr& operator=(RefPtr&& other)
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

    friend bool operator==(const RefPtr& lhs, const RefPtr& rhs)
    {
        return lhs.m_ptr == rhs.m_ptr;
    }
    friend bool operator!=(const RefPtr& lhs, const RefPtr& rhs)
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
