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
    void add_child(String name, std::unique_ptr<Highlighter>&& hl, bool override = false) override;
    void remove_child(StringView id) override;

    Highlighter& get_child(StringView path) override;

    Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const override;

    void fill_unique_ids(Vector<StringView>& unique_ids) const override;

protected:
    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override;
    void do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const override;

    using HighlighterMap = HashMap<String, std::unique_ptr<Highlighter>, MemoryDomain::Highlight>;
    HighlighterMap m_highlighters;
};

class Highlighters : public SafeCountable
{
public:
    Highlighters(Highlighters& parent) : SafeCountable{}, m_parent{&parent}, m_group{HighlightPass::All} {}

    HighlighterGroup& group() { return m_group; }
    const HighlighterGroup& group() const { return m_group; }

    void highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range);
    void compute_display_setup(HighlightContext context, DisplaySetup& setup) const;

private:
    friend class Scope;
    Highlighters() : m_group{HighlightPass::All} {}

    SafePtr<Highlighters> m_parent;
    HighlighterGroup m_group;
};

struct SharedHighlighters : public HighlighterGroup,
                            public Singleton<SharedHighlighters>
{
    SharedHighlighters() : HighlighterGroup{HighlightPass::All} {}
};

}

#endif // highlighter_group_hh_INCLUDED
