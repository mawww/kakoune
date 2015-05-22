#ifndef unit_tests_hh_INCLUDED
#define unit_tests_hh_INCLUDED

namespace Kakoune
{

struct UnitTest;
extern UnitTest* unit_tests;

struct UnitTest
{
    UnitTest(void (*func)()) : func(func), next(unit_tests) { unit_tests = this; }
    void (*func)();
    const UnitTest* next;
};

void run_unit_tests();

}

#endif // unit_tests_hh_INCLUDED
