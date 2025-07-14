#ifndef function_hh_INCLUDED
#define function_hh_INCLUDED

#include "exception.hh"
#include <utility>
#include <type_traits>

namespace Kakoune
{

template<typename Res, typename... Args>
struct FunctionVTable
{
    Res (*call)(void*, Args...);
    void* (*clone)(void*);
    void (*destroy)(void*);
};

template<typename Res, typename... Args>
inline constexpr FunctionVTable<Res, Args...> unset_vtable{
    .call = [](void*, Args...) -> Res { throw runtime_error("called an empty function"); },
    .clone = [](void*) -> void* { return nullptr; },
    .destroy = [](void*) {},
};

template<typename Target, typename Res, typename... Args>
inline constexpr FunctionVTable<Res, Args...> vtable_for{
    .call = [](void* target, Args... args) -> Res { return (*reinterpret_cast<Target*>(target))(std::forward<Args>(args)...); },
    .clone = [](void* target) -> void* { return new Target(*reinterpret_cast<Target*>(target)); },
    .destroy = [](void* target) { delete reinterpret_cast<Target*>(target); }
};

template<typename Res, typename... Args>
inline constexpr FunctionVTable<Res, Args...> vtable_for_func{
    .call = [](void* target, Args... args) -> Res { return (*reinterpret_cast<Res (*)(Args...)>(target))(std::forward<Args>(args)...); },
    .clone = [](void* target) -> void* { return target; },
    .destroy = [](void* target) {}
};

template<typename F>
class Function;

template<typename Res, typename... Args>
class Function<Res (Args...)>
{
public:
    Function() = default;

    template<typename Target>
        requires (not std::is_same_v<Function, std::remove_cvref_t<Target>>)
    Function(Target&& target)
    {
        using EffectiveTarget = std::remove_cvref_t<Target>;
        if constexpr (std::is_convertible_v<EffectiveTarget, Res(*)(Args...)>)
        {
            m_target = reinterpret_cast<void*>(static_cast<Res(*)(Args...)>(target));
            m_vtable = &vtable_for_func<Res, Args...>;
        }
        else
        {
            m_target = new EffectiveTarget(std::forward<Target>(target));
            m_vtable = &vtable_for<EffectiveTarget, Res, Args...>;
        }
    }

    Function(const Function& other) : m_target(other.m_vtable->clone(other.m_target)), m_vtable(other.m_vtable)
    {
    }

    Function(Function&& other) : m_target(other.m_target), m_vtable(other.m_vtable)
    {
        other.m_target = nullptr;
        other.m_vtable = &unset_vtable<Res, Args...>;
    }

    ~Function()
    {
        m_vtable->destroy(m_target);
    }

    Function& operator=(const Function& other)
    {
        m_target = other.m_vtable->clone(other.m_target);
        m_vtable = other.m_vtable;
        return *this;
    }

    Function& operator=(Function&& other)
    {
        m_target = other.m_target;
        m_vtable = other.m_vtable;
        other.m_target = nullptr;
        other.m_vtable = &unset_vtable<Res, Args...>;
        return *this;
    }

    Res operator()(Args... args) const
    {
        return m_vtable->call(m_target, std::forward<Args>(args)...);
    }

    explicit operator bool() const { return m_vtable != &unset_vtable<Res, Args...>; }

private:
    void* m_target = nullptr;
    const FunctionVTable<Res, Args...>* m_vtable = &unset_vtable<Res, Args...>;
};

}

#endif // function_hh_INCLUDED
