#ifndef highlighter_group_hh_INCLUDED
#define highlighter_group_hh_INCLUDED

#include "exception.hh"
#include "hash_map.hh"
#include "highlighter.hh"
#include "utils.hh"
#include "safe_ptr.hh"

namespace Kakoune
{

struct child_not_found : public runtime_error
{
    using runtime_error::runtime_error;
};

class HighlighterGroup : public Highlighter
{
public:
    HighlighterGroup(HighlightPass passes) : Highlighter{passes} {}

    bool has_children() const override { return true; }
    void add_child(HighlighterAndId&& hl) override;
    void remove_child(StringView id) override;

    Highlighter& get_child(StringView path) override;

    Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const override;

protected:
    void do_highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) override;
    void do_compute_display_setup(const Context& context, HighlightPass pass, DisplaySetup& setup) override;

    using HighlighterMap = HashMap<String, std::unique_ptr<Highlighter>, MemoryDomain::Highlight>;
    HighlighterMap m_highlighters;
};

class Highlighters : public HighlighterGroup, public SafeCountable
{
public:
    Highlighters(Highlighters& parent) : HighlighterGroup{HighlightPass::All}, SafeCountable{}, m_parent(&parent) {}

private:
    void do_highlight(const Context& context, HighlightPass pass, DisplayBuffer& display_buffer, BufferRange range) override;
    void do_compute_display_setup(const Context& context, HighlightPass pass, DisplaySetup& setup) override;

    friend class Scope;
    Highlighters() : HighlighterGroup{HighlightPass::All} {}

    SafePtr<Highlighters> m_parent;
};

struct DefinedHighlighters : public HighlighterGroup,
                             public Singleton<DefinedHighlighters>
{
    DefinedHighlighters() : HighlighterGroup{HighlightPass::All} {}
};

}

#endif // highlighter_group_hh_INCLUDED
