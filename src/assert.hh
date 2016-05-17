#ifndef assert_hh_INCLUDED
#define assert_hh_INCLUDED

namespace Kakoune
{

class StringView;

// return true if user asked to ignore the error
bool notify_fatal_error(StringView message);

void on_assert_failed(const char* message);

}

#define STRINGIFY(X) #X
#define TOSTRING(X) STRINGIFY(X)

#ifdef KAK_DEBUG
    #define kak_assert(...) \
        if (not (__VA_ARGS__)) \
            on_assert_failed("assert failed \"" #__VA_ARGS__ \
                             "\" at " __FILE__ ":" TOSTRING(__LINE__))
#else
    #define kak_assert(...)
#endif


#endif // assert_hh_INCLUDED
