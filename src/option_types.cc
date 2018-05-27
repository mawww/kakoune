#include "option_types.hh"
#include "unit_tests.hh"

namespace Kakoune
{

UnitTest test_option_parsing{[]{
    auto check = [](auto&& value, StringView str)
    {
        auto repr = option_to_string(value);
        kak_assert(repr == str);
        auto parsed = option_from_string(Meta::Type<std::decay_t<decltype(value)>>{}, str);
        kak_assert(parsed == value);
    };

    check(123, "123");
    check(true, "true");
    check(Vector<String>{"foo", "bar:", "baz"}, "foo:bar\\::baz");
    check(HashMap<String, int>{{"foo", 10}, {"b=r", 20}, {"b:z", 30}}, "foo=10:b\\=r=20:b\\:z=30");
    check(DebugFlags::Keys | DebugFlags::Hooks, "hooks|keys");
}};

}
