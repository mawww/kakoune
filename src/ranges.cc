#include "ranges.hh"
#include "unit_tests.hh"
#include "string.hh"
#include "string_utils.hh"

namespace Kakoune
{

UnitTest test_ranges{[] {
    using Strs = ConstArrayView<StringView>;
    auto check_equal = [](auto&& container, auto&& expected) {
        kak_assert(std::equal(container.begin(), container.end(), expected.begin(), expected.end()));
    };
    check_equal("a,b,c"_sv | split<StringView>(','), Strs{"a", "b", "c"});
    check_equal(",b,c"_sv  | split<StringView>(','), Strs{"", "b", "c"});
    check_equal(",b,"_sv   | split<StringView>(','), Strs{"", "b", ""});
    check_equal(","_sv     | split<StringView>(','), Strs{"", ""});
    check_equal(""_sv      | split<StringView>(','), Strs{});

    check_equal("a,b,c,"_sv | split_after<StringView>(','), Strs{"a,", "b,", "c,"});
    check_equal("a,b,c"_sv  | split_after<StringView>(','), Strs{"a,", "b,", "c"});

    check_equal(R"(a\,,\,b,\,)"_sv | split<StringView>(',', '\\')
                                   | transform(unescape<',', '\\'>), Strs{"a,", ",b", ","});
    check_equal(R"(\,\,)"_sv | split<StringView>(',', '\\')
                             | transform(unescape<',', '\\'>), Strs{",,"});
    check_equal(R"(\\,\\,)"_sv | split<StringView>(',', '\\')
                               | transform(unescape<',', '\\'>), Strs{R"(\)", R"(\)", ""});

    check_equal(Array{""_sv, "abc"_sv, ""_sv, "def"_sv, ""_sv} | flatten(), "abcdef"_sv);
    check_equal(Vector<StringView>{"", ""} | flatten(), ""_sv);
    check_equal(Vector<StringView>{} | flatten(), ""_sv);
}};

}
