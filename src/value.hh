#ifndef value_hh_INCLUDED
#define value_hh_INCLUDED

#include "unordered_map.hh"
#include "units.hh"

#include <memory>

namespace Kakoune
{

struct bad_value_cast {};

struct Value
{
    Value() = default;

    template<typename T>
    Value(T&& val) : m_value{new Model<T>{std::forward<T>(val)}} {}

    Value(const Value& val) = delete;
    Value(Value&&) = default;

    Value& operator=(const Value& val) = delete;
    Value& operator=(Value&& val) = default;

    explicit operator bool() const { return (bool)m_value; }

    template<typename T>
    bool is_a() const
    {
        return m_value and m_value->type() == typeid(T);
    }

    template<typename T>
    T& as()
    {
        if (not is_a<T>())
            throw bad_value_cast{};
        return static_cast<Model<T>*>(m_value.get())->m_content;
    }

    template<typename T>
    const T& as() const
    {
        return const_cast<Value*>(this)->as<T>();
    }

private:
    struct Concept
    {
        virtual ~Concept() {}
        virtual const std::type_info& type() const = 0;
    };

    template<typename T>
    struct Model : public Concept
    {
        Model(T&& val) : m_content(std::move(val)) {}
        const std::type_info& type() const override { return typeid(T); }

        T m_content;

        using Alloc = Allocator<Model<T>, MemoryDomain::Values>;
        static void* operator new (std::size_t sz) { return Alloc{}.allocate(1); }
        static void operator delete (void* ptr) { Alloc{}.deallocate((Model<T>*)ptr, 1); }
    };

    std::unique_ptr<Concept> m_value;
};

struct ValueId : public StronglyTypedNumber<ValueId, int>
{
    constexpr ValueId(int value = 0) : StronglyTypedNumber(value) {}

    static ValueId get_free_id()
    {
        static ValueId next;
        return next++;
    }
};

inline size_t hash_value(ValueId val) { return hash_value((int)val); }

using ValueMap = UnorderedMap<ValueId, Value, MemoryDomain::Values>;

}

#endif // value_hh_INCLUDED
