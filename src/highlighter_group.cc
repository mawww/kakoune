#include "highlighter_group.hh"

#include "ranges.hh"
#include "string_utils.hh"

namespace Kakoune
{

void HighlighterGroup::do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range)
{
    for (auto& hl : m_highlighters)
        hl.value->highlight(context, display_buffer, range);
}

void HighlighterGroup::do_compute_display_setup(HighlightContext context, DisplaySetup& setup) const
{
    for (auto& hl : m_highlighters)
        hl.value->compute_display_setup(context, setup);
}

void HighlighterGroup::fill_unique_ids(Vector<StringView>& unique_ids) const
{
    for (auto& hl : m_highlighters)
        hl.value->fill_unique_ids(unique_ids);
}

void HighlighterGroup::add_child(String name, std::unique_ptr<Highlighter>&& hl, bool override)
{
    if ((hl->passes() & passes()) != hl->passes())
        throw runtime_error{"cannot add that highlighter to this group, passes don't match"};

    auto it = m_highlighters.find(name);
    if (it != m_highlighters.end())
    {
        if (not override)
            throw runtime_error(format("duplicate id: '{}'", name));
        it->value = std::move(hl);
        return;
    }

    m_highlighters.insert({std::move(name), std::move(hl)});
}

void HighlighterGroup::remove_child(StringView id)
{
    m_highlighters.remove(id);
}

Highlighter& HighlighterGroup::get_child(StringView path)
{
    auto sep_it = find(path, '/');
    StringView id(path.begin(), sep_it);
    auto it = m_highlighters.find(id);
    if (it == m_highlighters.end())
        throw child_not_found(format("no such id: '{}'", id));
    if (sep_it == path.end())
        return *it->value;
    else
        return it->value->get_child({sep_it+1, path.end()});
}

Completions HighlighterGroup::complete_child(StringView path, ByteCount cursor_pos, bool group) const
{
    auto sep_it = find(path, '/');
    if (sep_it != path.end())
    {
        ByteCount offset = sep_it+1 - path.begin();
        Highlighter& hl = const_cast<HighlighterGroup*>(this)->get_child({path.begin(), sep_it});
        return offset_pos(hl.complete_child(path.substr(offset), cursor_pos - offset, group), offset);
    }

    auto candidates = complete(
        path, cursor_pos,
        m_highlighters | filter([=](auto& hl) { return not group or hl.value->has_children(); })
                       | transform([](auto& hl) { return hl.value->has_children() ? hl.key + "/" : hl.key; })
                       | gather<Vector<String>>());

    auto completions_flags = group ? Completions::Flags::None : Completions::Flags::Menu;
    return { 0, 0, std::move(candidates), completions_flags };
}

void Highlighters::highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range)
{
    Vector<StringView> disabled_ids(context.disabled_ids.begin(), context.disabled_ids.end());
    m_group.fill_unique_ids(disabled_ids);

    if (m_parent)
        m_parent->highlight({context.context, context.setup, context.pass, disabled_ids}, display_buffer, range);
    m_group.highlight(context, display_buffer, range);
}

void Highlighters::compute_display_setup(HighlightContext context, DisplaySetup& setup) const
{
    Vector<StringView> disabled_ids(context.disabled_ids.begin(), context.disabled_ids.end());
    m_group.fill_unique_ids(disabled_ids);

    if (m_parent)
        m_parent->compute_display_setup({context.context, context.setup, context.pass, disabled_ids}, setup);
    m_group.compute_display_setup(context, setup);
}

}
