#ifndef unit_tests_hh_INCLUDED
#define unit_tests_hh_INCLUDED

namespace Kakoune
{

struct UnitTest
{
#ifdef KAK_DEBUG
    UnitTest(void (*func)()) : func(func), next(list) { list = this; }
    void (*func)();
    const UnitTest* next;

    static void run_all_tests();
    static UnitTest* list;
#else
    UnitTest(void (*func)()) {}
#endif
};

}

#endif // unit_tests_hh_INCLUDED
