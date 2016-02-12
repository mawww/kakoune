#ifndef face_registry_hh_INCLUDED
#define face_registry_hh_INCLUDED

#include "face.hh"
#include "utils.hh"
#include "completion.hh"
#include "unordered_map.hh"

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
private:
    struct FaceOrAlias
    {
        Face face;
        String alias;

        FaceOrAlias(Face face = Face{}) : face(face) {}
    };

    using AliasMap = UnorderedMap<String, FaceOrAlias, MemoryDomain::Faces>;
    AliasMap m_aliases;
};

inline Face get_face(const String& facedesc)
{
    if (FaceRegistry::has_instance())
        return FaceRegistry::instance()[facedesc];
    return Face{};
}

}

#endif // face_registry_hh_INCLUDED
