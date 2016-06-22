#ifndef optional_hh_INCLUDED
#define optional_hh_INCLUDED

#include "assert.hh"

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
        noexcept(noexcept(new (nullptr) T(std::move(other.m_value))))
        : m_valid(other.m_valid)
    {
        if (m_valid)
            new (&m_value) T(std::move(other.m_value));
    }

    Optional& operator=(const Optional& other)
    {
        destruct_ifn();
        if ((m_valid = other.m_valid))
            new (&m_value) T(other.m_value);
        return *this;
    }

    Optional& operator=(Optional&& other)
    {
        destruct_ifn();
        if ((m_valid = other.m_valid))
            new (&m_value) T(std::move(other.m_value));
        return *this;
    }

    ~Optional() { destruct_ifn(); }

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

    template<typename... Args>
    void emplace(Args&&... args)
    {
        destruct_ifn();
        new (&m_value) T{std::forward<Args>(args)...};
        m_valid = true;
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

    template<typename U>
    T value_or(U&& fallback) const { return m_valid ? m_value : T{fallback}; }

private:
    void destruct_ifn() { if (m_valid) m_value.~T(); }

    struct Empty {};
    union
    {
        Empty m_empty; // disable default construction of value
        T m_value;
    };
    bool m_valid;
};

}

#endif // optional_hh_INCLUDED
