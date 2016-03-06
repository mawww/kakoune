#ifndef value_hh_INCLUDED
#define value_hh_INCLUDED

#include "unordered_map.hh"
#include "units.hh"

#include <type_traits>
#include <memory>

namespace Kakoune
{

struct bad_value_cast {};

struct Value
{
    Value() = default;

    template<typename T,
             typename = typename std::enable_if<not std::is_same<Value, T>::value>::type>
    Value(T&& val)
        : m_value{new Model<typename std::remove_reference<T>::type>{std::forward<T>(val)}} {}

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
    struct Model : public Concept, public UseMemoryDomain<MemoryDomain::Values>
    {
        Model(T&& val) : m_content(std::move(val)) {}
        const std::type_info& type() const override { return typeid(T); }

        T m_content;
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

using ValueMap = UnorderedMap<ValueId, Value, MemoryDomain::Values>;

}

#endif // value_hh_INCLUDED
