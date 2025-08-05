#ifndef unique_ptr_hh_INCLUDED
#define unique_ptr_hh_INCLUDED

#include <utility>
#include <type_traits>
#include <cstddef>

namespace Kakoune
{

template<typename T>
class UniquePtr
{
    using Type = std::remove_extent_t<T>;

public:
    explicit UniquePtr(Type* ptr = nullptr) : m_ptr{ptr} {}
    UniquePtr(std::nullptr_t) : m_ptr{nullptr} {}

    UniquePtr(const UniquePtr&) = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    template<typename U> requires std::is_convertible_v<U*, T*>
    UniquePtr(UniquePtr<U>&& other) noexcept { m_ptr = other.release(); }

    template<typename U> requires std::is_convertible_v<U*, T*>
    UniquePtr& operator=(UniquePtr<U>&& other) noexcept(noexcept(std::declval<Type>().~Type()))
    {
        destroy();
        m_ptr = other.release();
        return *this;
    }
    ~UniquePtr() { destroy(); }

    Type* get() const { return m_ptr; }
    Type* operator->() const { return m_ptr; };
    Type& operator*() const { return *m_ptr; };
    Type& operator[](size_t i) const requires std::is_array_v<T> { return m_ptr[i]; }
    Type* release() { auto ptr = m_ptr; m_ptr = nullptr; return ptr; }

    void reset(Type* ptr = nullptr) { destroy(); m_ptr = ptr; }

    explicit operator bool() const { return m_ptr != nullptr; }

    friend bool operator==(const UniquePtr&, const UniquePtr&) = default;
    friend bool operator==(const UniquePtr& lhs, const Type* rhs) { return lhs.get() == rhs; }

private:
    void destroy()
    {
        if constexpr (std::is_array_v<T>)
            delete[] m_ptr;
        else
            delete m_ptr;
        m_ptr = nullptr;
    }

    Type* m_ptr;
};

template<typename T, typename... Args>
UniquePtr<T> make_unique_ptr(Args&&... args)
{
    return UniquePtr<T>(new T(std::forward<Args>(args)...));
}

}

#endif // unique_ptr_hh_INCLUDED
