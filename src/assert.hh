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
#define COMMA ,

#ifdef KAK_DEBUG
    #define kak_assert(condition) \
        if (not (condition)) \
            on_assert_failed("assert failed \"" #condition \
                             "\" at " __FILE__ ":" TOSTRING(__LINE__))
#else
    #define kak_assert(condition)
#endif


#endif // assert_hh_INCLUDED
