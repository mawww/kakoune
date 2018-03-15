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
    #define kak_assert(...) do { \
        if (not (__VA_ARGS__)) \
            on_assert_failed("assert failed \"" #__VA_ARGS__ \
                             "\" at " __FILE__ ":" TOSTRING(__LINE__)); \
    } while (false)

    #define kak_expect_throw(exception_type, ...) try {\
        __VA_ARGS__; \
        on_assert_failed("expression \"" #__VA_ARGS__ \
                         "\" did not throw \"" #exception_type \
                         "\" at " __FILE__ ":" TOSTRING(__LINE__)); \
    } catch (exception_type &err) {}
#else
    #define kak_assert(...) do { (void)sizeof(__VA_ARGS__); } while(false)
    #define kak_expect_throw(_, ...) do { (void)sizeof(__VA_ARGS__); } while(false)
#endif


#endif // assert_hh_INCLUDED
