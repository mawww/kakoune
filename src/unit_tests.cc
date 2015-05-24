#include "unit_tests.hh"

#include "assert.hh"
#include "diff.hh"
#include "utf8.hh"
#include "string.hh"

namespace Kakoune
{

UnitTest test_utf8{[]()
{
    StringView str = "maïs mélange bientôt";
    kak_assert(utf8::distance(std::begin(str), std::end(str)) == 20);
    kak_assert(utf8::codepoint(std::begin(str) + 2, std::end(str)) == 0x00EF);
}};

UnitTest test_diff{[]()
{
    auto eq = [](const Diff& lhs, const Diff& rhs) {
        return lhs.mode == rhs.mode and lhs.len == rhs.len and lhs.posB == rhs.posB;
    };

    {
        StringView s1 = "mais que fais la police";
        StringView s2 = "mais ou va la police";

        auto diff = find_diff(s1.begin(), (int)s1.length(), s2.begin(), (int)s2.length());
        kak_assert(diff.size() == 11);
    }

    {
        StringView s1 = "a?";
        StringView s2 = "!";

        auto diff = find_diff(s1.begin(), (int)s1.length(), s2.begin(), (int)s2.length());

        kak_assert(diff.size() == 3 and
                   eq(diff[0], {Diff::Remove, 1, 0}) and
                   eq(diff[1], {Diff::Add, 1, 0}) and
                   eq(diff[2], {Diff::Remove, 1, 0}));
    }
}};

UnitTest* UnitTest::list = nullptr;

void UnitTest::run_all_tests()
{
    for (const UnitTest* test = UnitTest::list; test; test = test->next)
        test->func();
}

}
