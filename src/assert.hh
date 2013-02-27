#ifndef assert_hh_INCLUDED
#define assert_hh_INCLUDED

namespace Kakoune
{

void on_assert_failed(const char* message);

}

#define STRINGIFY(X) #X
#define TOSTRING(X) STRINGIFY(X)
#define COMMA ,

#ifdef assert
#undef assert
#endif

#ifdef KAK_DEBUG
    #define assert(condition) \
        if (not (condition)) \
            on_assert_failed("assert failed \"" #condition \
                             "\" at " __FILE__ ":" TOSTRING(__LINE__))
#else
    #define assert(condition)
#endif


#endif // assert_hh_INCLUDED
