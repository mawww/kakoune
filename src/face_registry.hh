#ifndef face_registry_hh_INCLUDED
#define face_registry_hh_INCLUDED

#include "face.hh"
#include "utils.hh"
#include "completion.hh"

#include <unordered_map>

namespace Kakoune
{

class FaceRegistry : public Singleton<FaceRegistry>
{
public:
    FaceRegistry();

    const Face& operator[](const String& facedesc);
    void register_alias(const String& name, const String& facedesc,
                        bool override = false);

    CandidateList complete_alias_name(StringView prefix,
                                      ByteCount cursor_pos) const;
private:
    std::unordered_map<String, Face> m_aliases;
};

inline const Face& get_face(const String& facedesc)
{
    return FaceRegistry::instance()[facedesc];
}

}

#endif // face_registry_hh_INCLUDED

