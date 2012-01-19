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
    void remove(const std::string& id);

    HighlighterGroup& get_group(const std::string& id);

    CandidateList complete_id(const std::string& prefix, size_t cursor_pos);
    CandidateList complete_group_id(const std::string& prefix, size_t cursor_pos);

private:
    idvaluemap<std::string, HighlighterFunc> m_highlighters;
};

}

#endif // highlighter_group_hh_INCLUDED
