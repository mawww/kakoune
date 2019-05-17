#include "path_manager.hh"

#include "client_manager.hh"

namespace Kakoune
{

class GlobType
{
public:
    static GlobType* resolve(StringView name);

    virtual bool matches(StringView name, StringView text) const = 0;
    virtual Vector<String> expand(StringView name) const = 0;
};

struct LiteralGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const
    {
        return name == text;
    }

    Vector<String> expand(StringView name) const
    {
        Vector<String> res;
        res.push_back(String{name});
        return res;
    }
} literal_glob_type{};

struct ClientNameGlobType : public GlobType
{
    bool matches(StringView name, StringView text) const
    {
        auto it = find_if(ClientManager::instance(), [&text](auto& client) { return client->context().name() == text; });
        return it != ClientManager::instance().end();
    }

    Vector<String> expand(StringView name) const
    {
        Vector<String> res;
        for (auto& client : ClientManager::instance())
            res.push_back(client->context().name());
        return res;
    }
} client_name_glob_type{};

GlobType* GlobType::resolve(StringView name)
{
    if (name == "$client_name")
        return &client_name_glob_type;
    return &literal_glob_type;
}

Glob::Glob(StringView name)
    : m_name{name}
{
}

bool Glob::matches(StringView text) const
{
    return GlobType::resolve(m_name)->matches(m_name, text);
}

Vector<String> Glob::expand() const
{
    return GlobType::resolve(m_name)->expand(m_name);
}

}
