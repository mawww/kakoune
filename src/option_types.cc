#include "option_types.hh"
#include "unit_tests.hh"

namespace Kakoune
{

UnitTest test_option_parsing{[]{
    auto check = [](auto&& value, ConstArrayView<String> strs)
    {
        auto repr = option_to_strings(value);
        kak_assert(strs == ConstArrayView<String>{repr});
        auto parsed = option_from_strings(Meta::Type<std::remove_cvref_t<decltype(value)>>{}, strs);
        kak_assert(parsed == value);
    };

    check(123, {"123"});
    check(true, {"true"});
    check(Vector<String>{"foo", "bar:", "baz"}, {"foo", "bar:", "baz"});
    check(Vector<int>{10, 20, 30}, {"10", "20", "30"});
    check(HashMap<String, int>{{"foo", 10}, {"b=r", 20}, {"b:z", 30}}, {"foo=10", "b\\=r=20", "b:z=30"});
    check(DebugFlags::Keys | DebugFlags::Hooks, {"hooks|keys"});
}};

}
