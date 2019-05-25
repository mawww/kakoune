#ifndef session_manager_hh_INCLUDED
#define session_manager_hh_INCLUDED

#include "exception.hh"
#include "string.hh"
#include "utils.hh"
#include "vector.hh"

namespace Kakoune
{

struct disconnected : runtime_error
{
    using runtime_error::runtime_error;
};

class Session
{
public:
    Session(StringView name);

    inline const String& name() const         { return m_name; }
    inline String root() const                { return root(m_name); }
    inline String file(StringView path) const { return file(m_name, path); }

    int listen();
    int connect();
    bool check();
    void rename(StringView name);
    void remove();

    String read(StringView path) const;
    void write(StringView path, StringView data) const;
    bool unlink(StringView path) const;

private:
    static String root(StringView session);
    static String file(StringView session, StringView path);

    String m_name;
};

class SessionManager : public Singleton<SessionManager>
{
public:
    SessionManager(StringView session_name)
        : m_session{session_name}
    {}

    inline Session& get() { return m_session; }
    Vector<String> list();

private:
    Session m_session;
};

}

#endif // session_manager_hh_INCLUDED
