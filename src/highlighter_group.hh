#ifndef highlighter_group_hh_INCLUDED
#define highlighter_group_hh_INCLUDED

#include "highlighter.hh"
#include "idvaluemap.hh"

namespace Kakoune
{

class DisplayBuffer;
class Window;

class HighlighterGroup
{
public:
    void operator()(DisplayBuffer& display_buffer);

    void append(HighlighterAndId&& highlighter);
    void remove(const String& id);

    HighlighterGroup& get_group(const String& id);

    CandidateList complete_id(const String& prefix, size_t cursor_pos);
    CandidateList complete_group_id(const String& prefix, size_t cursor_pos);

private:
    idvaluemap<String, HighlighterFunc> m_highlighters;
};

}

#endif // highlighter_group_hh_INCLUDED
