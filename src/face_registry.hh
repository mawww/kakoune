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

    Face operator[](StringView facedesc);
    void register_alias(StringView name, StringView facedesc,
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

inline Face get_face(StringView facedesc)
{
    if (FaceRegistry::has_instance())
        return FaceRegistry::instance()[facedesc];
    return Face{};
}

String to_string(Face face);

}

#endif // face_registry_hh_INCLUDED
