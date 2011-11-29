#include "highlighter_registry.hh"

#include "exception.hh"
#include "window.hh"

namespace Kakoune
{

struct factory_not_found : public runtime_error
{
    factory_not_found() : runtime_error("factory not found") {}
};

void HighlighterRegistry::register_factory(const std::string& name,
                                           const HighlighterFactory& factory)
{
    assert(m_factories.find(name) == m_factories.end());
    m_factories[name] = factory;
}

void HighlighterRegistry::add_highlighter_to_window(Window& window,
                                                    const std::string& name,
                                                    const HighlighterParameters& parameters)
{
    auto it = m_factories.find(name);
    if (it == m_factories.end())
        throw factory_not_found();

    window.add_highlighter(it->second(window, parameters));
}

CandidateList HighlighterRegistry::complete_highlighter(const std::string& prefix,
                                                        size_t cursor_pos)
{
    std::string real_prefix = prefix.substr(0, cursor_pos);
    CandidateList result;
    for (auto& highlighter : m_factories)
    {
        if (highlighter.first.substr(0, real_prefix.length()) == real_prefix)
            result.push_back(highlighter.first);
    }
    return result;
}

}
