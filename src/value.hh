#ifndef value_hh_INCLUDED
#define value_hh_INCLUDED

#include "hash_map.hh"
#include "meta.hh"
#include "unique_ptr.hh"

#include <type_traits>

namespace Kakoune
{

struct bad_value_cast {};

struct Value
{
    Value() = default;

    template<typename T> requires (not std::is_same_v<Value, T>)
    Value(T&& val)
        : m_value{new Model<std::remove_cvref_t<T>>{std::forward<T>(val)}} {}

    template<typename T>
    Value(Meta::Type<T>, auto&&... args) :
        m_value(new Model<T>(std::forward<decltype(args)>(args)...)) {}

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
        virtual ~Concept() = default;
        virtual const std::type_info& type() const = 0;
    };

    template<typename T>
    struct Model : public Concept, public UseMemoryDomain<MemoryDomain::Values>
    {
        Model(auto&&... args) : m_content(std::forward<decltype(args)>(args)...) {}
        const std::type_info& type() const override { return typeid(T); }

        T m_content;
    };

    UniquePtr<Concept> m_value;
};

enum class ValueId : int {};

inline ValueId get_free_value_id()
{
    static int next = 0;
    return (ValueId)(next++);
}

using ValueMap = HashMap<ValueId, Value, MemoryDomain::Values>;

}

#endif // value_hh_INCLUDED
