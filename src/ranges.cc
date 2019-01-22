#include "ranges.hh"
#include "unit_tests.hh"
#include "string.hh"
#include "string_utils.hh"

namespace Kakoune
{

UnitTest test_ranges{[] {
    auto check_equal = [](auto&& container, ConstArrayView<StringView> expected) {
        kak_assert(std::equal(container.begin(), container.end(), expected.begin(), expected.end()));
    };
    check_equal("a,b,c"_sv | split<StringView>(','), {"a", "b", "c"});
    check_equal(",b,c"_sv  | split<StringView>(','), {"", "b", "c"});
    check_equal(",b,"_sv   | split<StringView>(','), {"", "b", ""});
    check_equal(","_sv     | split<StringView>(','), {"", ""});
    check_equal(""_sv      | split<StringView>(','), {});

    check_equal("a,b,c,"_sv | split_after<StringView>(','), {"a,", "b,", "c,"});
    check_equal("a,b,c"_sv  | split_after<StringView>(','), {"a,", "b,", "c"});

    check_equal(R"(a\,,\,b,\,)"_sv | split<StringView>(',', '\\')
                                   | transform(unescape<',', '\\'>), {"a,", ",b", ","});
    check_equal(R"(\,\,)"_sv | split<StringView>(',', '\\')
                             | transform(unescape<',', '\\'>), {",,"});
    check_equal(R"(\\,\\,)"_sv | split<StringView>(',', '\\')
                               | transform(unescape<',', '\\'>), {R"(\)", R"(\)", ""});
}};

}
