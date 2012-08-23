#ifndef highlighter_registry_h_INCLUDED
#define highlighter_registry_h_INCLUDED

#include "string.hh"
#include <unordered_map>

#include "highlighter.hh"
#include "utils.hh"
#include "completion.hh"
#include "idvaluemap.hh"

namespace Kakoune
{

class Window;
class HighlighterGroup;

typedef std::function<HighlighterAndId (Window& window,
                                        const HighlighterParameters& params)> HighlighterFactory;

class HighlighterRegistry : public Singleton<HighlighterRegistry>
{
public:
    void register_factory(const String& name,
                          const HighlighterFactory& factory);

    void add_highlighter_to_group(Window& window,
                                  HighlighterGroup& group,
                                  const String& factory_name,
                                  const HighlighterParameters& parameters);

    CandidateList complete_highlighter(const String& prefix,
                                      CharCount cursor_pos);

private:
    idvaluemap<String, HighlighterFactory> m_factories;
};

}

#endif // highlighter_registry_h_INCLUDED
