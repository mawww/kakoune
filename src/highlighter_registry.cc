#include "highlighter_registry.hh"

#include "exception.hh"
#include "window.hh"
#include "highlighters.hh"

namespace Kakoune
{

struct factory_not_found : public runtime_error
{
    factory_not_found(const String& name)
        : runtime_error("highlighter factory not found '" + name + "'") {}
};

void HighlighterRegistry::register_factory(const String& name,
                                           const HighlighterFactory& factory)
{
    assert(not m_factories.contains(name));
    m_factories.append(std::make_pair(name, factory));
}

void HighlighterRegistry::add_highlighter_to_group(Window& window,
                                                   HighlighterGroup& group,
                                                   const String& name,
                                                   const HighlighterParameters& parameters)
{
    auto it = m_factories.find(name);
    if (it == m_factories.end())
        throw factory_not_found(name);

    group.append(it->second(window, parameters));
}

CandidateList HighlighterRegistry::complete_highlighter(const String& prefix,
                                                        size_t cursor_pos)
{
    return m_factories.complete_id<str_to_str>(prefix, cursor_pos);
}

}
