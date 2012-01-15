#ifndef highlighter_registry_h_INCLUDED
#define highlighter_registry_h_INCLUDED

#include <string>
#include <unordered_map>

#include "highlighter.hh"
#include "utils.hh"
#include "completion.hh"
#include "idvaluemap.hh"

namespace Kakoune
{

class Window;

typedef std::function<HighlighterAndId (Window& window,
                                        const HighlighterParameters& params)> HighlighterFactory;

class HighlighterRegistry : public Singleton<HighlighterRegistry>
{
public:
    void register_factory(const std::string& name,
                          const HighlighterFactory& factory);

    void add_highlighter_to_window(Window& window,
                                  const std::string& factory_name,
                                  const HighlighterParameters& parameters);

    CandidateList complete_highlighter(const std::string& prefix,
                                      size_t cursor_pos);

private:
    idvaluemap<std::string, HighlighterFactory> m_factories;
};

}

#endif // highlighter_registry_h_INCLUDED
