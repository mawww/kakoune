#ifndef highlighters_hh_INCLUDED
#define highlighters_hh_INCLUDED

#include "highlighter.hh"
#include "idvaluemap.hh"

namespace Kakoune
{

void register_highlighters();

class DisplayBuffer;
class Window;

class HighlighterGroup
{
public:
    void operator()(DisplayBuffer& display_buffer);

    void add_highlighter(HighlighterAndId&& highlighter);
    void remove_highlighter(const std::string& id);

    CandidateList complete_highlighterid(const std::string& prefix,
                                         size_t cursor_pos);

    static HighlighterAndId create(Window& window,
                                   const HighlighterParameters& params);
private:
    idvaluemap<std::string, HighlighterFunc> m_highlighters;
};

}

#endif // highlighters_hh_INCLUDED
