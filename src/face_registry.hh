#ifndef face_registry_hh_INCLUDED
#define face_registry_hh_INCLUDED

#include "face.hh"
#include "utils.hh"
#include "completion.hh"
#include "hash_map.hh"

namespace Kakoune
{

class FaceRegistry : public Singleton<FaceRegistry>
{
public:
    FaceRegistry();

    Face operator[](const String& facedesc);
    void register_alias(const String& name, const String& facedesc,
                        bool override = false);

    CandidateList complete_alias_name(StringView prefix,
                                      ByteCount cursor_pos) const;

    struct FaceOrAlias
    {
        Face face = {};
        String alias = {};
    };

    using AliasMap = HashMap<String, FaceOrAlias, MemoryDomain::Faces>;
    const AliasMap &aliases() const { return m_aliases; }

private:
    AliasMap m_aliases;
};

inline Face get_face(const String& facedesc)
{
    if (FaceRegistry::has_instance())
        return FaceRegistry::instance()[facedesc];
    return Face{};
}

String to_string(Face face);

}

#endif // face_registry_hh_INCLUDED
