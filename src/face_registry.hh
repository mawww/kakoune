#pragma once

#include "face.hh"
#include "utils.hh"
#include "hash_map.hh"
#include "ranges.hh"
#include "string.hh"
#include "safe_ptr.hh"

namespace Kakoune
{

class FaceRegistry : public SafeCountable
{
public:
    FaceRegistry(FaceRegistry& parent) : SafeCountable{}, m_parent(&parent) {}

    Face operator[](StringView facedesc) const;
    void add_face(StringView name, StringView facedesc, bool override = false);
    void remove_face(StringView name);

    struct FaceOrAlias
    {
        Face face = {};
        String alias = {};
    };
    using FaceMap = HashMap<String, FaceOrAlias, MemoryDomain::Faces>;

    auto flatten_faces() const
    {
        auto merge = [](auto&& first, const FaceMap& second) {
            return concatenated(std::forward<decltype(first)>(first)
                                | filter([&second](auto& i) { return not second.contains(i.key); }),
                                second);
        };
        static const FaceMap empty;
        auto& parent = m_parent ? m_parent->m_faces : empty;
        auto& grand_parent = (m_parent and m_parent->m_parent) ? m_parent->m_parent->m_faces : empty;
        return merge(merge(grand_parent, parent), m_faces);
    }

private:
    friend class Scope;
    FaceRegistry();

    SafePtr<FaceRegistry> m_parent;
    FaceMap m_faces;
};

String to_string(Face face);

}
