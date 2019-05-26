#include "session_manager.hh"

#include "exception.hh"
#include "file.hh"
#include "string_utils.hh"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <ftw.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

namespace Kakoune
{

static String get_user_name()
{
    auto pw = getpwuid(geteuid());
    if (pw)
      return pw->pw_name;
    return getenv("USER");
}

Session::Session(StringView name)
    : m_name{name.str()}
{
    if (not all_of(name, is_identifier))
        throw runtime_error{format("invalid session name: '{}'", name)};
}

String Session::root(StringView name)
{
    auto slash_count = std::count(name.begin(), name.end(), '/');
    if (slash_count > 1)
        throw runtime_error{"session names are either <user>/<name> or <name>"};
    else if (slash_count == 1)
        return format("{}/kakoune/{}", tmpdir(), name);
    else
        return format("{}/kakoune/{}/{}", tmpdir(), get_user_name(), name);
}

String Session::file(StringView name, StringView path)
{
    kak_assert(not path.empty());
    kak_assert(path[0] != '/');
    kak_assert(path[0] != '.');
    return format("{}/{}", root(name), path);
}

int Session::listen()
{
    // set sticky bit on the shared kakoune directory
    make_directory(format("{}/kakoune", tmpdir()), 01777);
    make_directory(root().c_str(), 0711);

    int listen_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(listen_sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, file("socket").c_str());

    // Do not give any access to the socket to other users by default
    auto old_mask = umask(0077);
    auto restore_mask = on_scope_end([old_mask]() { umask(old_mask); });

    if (bind(listen_sock, (sockaddr*) &addr, sizeof(sockaddr_un)) == -1)
       throw runtime_error(format("unable to bind listen socket '{}': {}",
                                  addr.sun_path, strerror(errno)));

    if (::listen(listen_sock, 4) == -1)
       throw runtime_error(format("unable to listen on socket '{}': {}",
                                  addr.sun_path, strerror(errno)));
    return listen_sock;
}

int Session::connect()
{
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    fcntl(sock, F_SETFD, FD_CLOEXEC);
    sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, file("socket").c_str());
    if (::connect(sock, (sockaddr*)&addr, sizeof(addr.sun_path)) == -1)
    {
        close(sock);
        throw disconnected(format("connect to {} failed", addr.sun_path));
    }
    return sock;
}

bool Session::check()
{
    try
    {
        close(connect());
        return true;
    }
    catch (const disconnected&)
    {
        return false;
    }
}

void Session::rename(StringView name)
{
    if (not all_of(name, is_identifier))
        throw runtime_error{format("invalid session name: '{}'", name)};
    if (::rename(root(m_name).c_str(), root(name).c_str()) != 0)
        throw runtime_error(format("unable to rename current session: '{}' may be already in use", name));
    m_name = name.str();
}

int remove_recursive_fn(const char* fpath, const struct stat* db, int typeflag, struct FTW* ftwbuf)
{
    switch (typeflag)
    {
    case FTW_F:
    case FTW_SL:
    case FTW_SLN:
        return unlink(fpath);
    case FTW_DP:
        return rmdir(fpath);
    default:
        return -1;
    }
}

void Session::remove()
{
    nftw(root().c_str(), remove_recursive_fn, 10, FTW_DEPTH|FTW_PHYS);
}

Vector<String> SessionManager::list()
{
    return list_files(format("{}/kakoune/{}/", tmpdir(), get_user_name()))
        | transform([](const String& name) { return replace(name, "/", ""); })
        | gather<Vector<String>>();
}

}
