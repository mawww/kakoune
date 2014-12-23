#ifndef highlighter_group_hh_INCLUDED
#define highlighter_group_hh_INCLUDED

#include "exception.hh"
#include "id_map.hh"
#include "highlighter.hh"
#include "utils.hh"

namespace Kakoune
{

struct child_not_found : public runtime_error
{
    using runtime_error::runtime_error;
};

class HighlighterGroup : public Highlighter
{
public:
    void highlight(const Context& context, HighlightFlags flags, DisplayBuffer& display_buffer, BufferRange range) override;

    bool has_children() const { return true; }
    void add_child(HighlighterAndId&& hl) override;
    void remove_child(StringView id) override;

    Highlighter& get_child(StringView path) override;

    Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const override;

private:
    using HighlighterMap = IdMap<std::unique_ptr<Highlighter>>;
    HighlighterMap m_highlighters;
};

struct DefinedHighlighters : public HighlighterGroup,
                             public Singleton<DefinedHighlighters>
{
};

}

#endif // highlighter_group_hh_INCLUDED
