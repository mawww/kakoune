#ifndef optional_hh_INCLUDED
#define optional_hh_INCLUDED

namespace Kakoune
{

template<typename T>
struct Optional
{
public:
    constexpr Optional() : m_valid(false) {}
    Optional(const T& other) : m_valid(true) { new (&m_value) T(other); }
    Optional(T&& other) : m_valid(true) { new (&m_value) T(std::move(other)); }

    Optional(const Optional& other)
        : m_valid(other.m_valid)
    {
        if (m_valid)
            new (&m_value) T(other.m_value);
    }

    Optional(Optional&& other)
        noexcept(noexcept(new ((void*)0) T(std::move(other.m_value))))
        : m_valid(other.m_valid)
    {
        if (m_valid)
            new (&m_value) T(std::move(other.m_value));
    }

    Optional& operator=(const Optional& other)
    {
        if (m_valid)
            m_value.~T();
        if ((m_valid = other.m_valid))
            new (&m_value) T(other.m_value);
        return *this;
    }

    Optional& operator=(Optional&& other)
    {
        if (m_valid)
            m_value.~T();
        if ((m_valid = other.m_valid))
            new (&m_value) T(std::move(other.m_value));
        return *this;
    }

    ~Optional()
    {
        if (m_valid)
            m_value.~T();
    }

    constexpr explicit operator bool() const noexcept { return m_valid; }

    bool operator==(const Optional& other) const
    {
        if (m_valid == other.m_valid)
        {
            if (m_valid)
                return m_value == other.m_value;
            return true;
        }
        return false;
    }

    T& operator*()
    {
        kak_assert(m_valid);
        return m_value;
    }
    const T& operator*() const { return *const_cast<Optional&>(*this); }

    T* operator->()
    {
        kak_assert(m_valid);
        return &m_value;
    }
    const T* operator->() const { return const_cast<Optional&>(*this).operator->(); }

private:
    bool m_valid;
    union { T m_value; };
};

}

#endif // optional_hh_INCLUDED

