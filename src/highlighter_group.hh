#ifndef highlighter_group_hh_INCLUDED
#define highlighter_group_hh_INCLUDED

#include "exception.hh"
#include "id_map.hh"
#include "highlighter.hh"
#include "utils.hh"

namespace Kakoune
{

struct group_not_found : public runtime_error
{
    using runtime_error::runtime_error;
};

class HighlighterGroup
{
public:
    void operator()(const Context& context,
                    HighlightFlags flags,
                    DisplayBuffer& display_buffer) const;

    void append(HighlighterAndId&& hl);
    void remove(StringView id);

    HighlighterGroup& get_group(StringView path);
    HighlighterFunc get_highlighter(StringView path) const;

    Completions complete_id(StringView path, ByteCount cursor_pos) const;
    Completions complete_group_id(StringView path, ByteCount cursor_pos) const;

private:
    id_map<HighlighterFunc> m_highlighters;
};

class HierachicalHighlighter
{
public:
    using GroupMap = id_map<HighlighterGroup>;
    using Callback = std::function<void (GroupMap& groups,
                                         const Context& context,
                                         HighlightFlags flags,
                                         DisplayBuffer& display_buffer)>;

    HierachicalHighlighter(Callback callback, GroupMap groups)
        : m_callback(std::move(callback)), m_groups(std::move(groups)) {}

    void operator()(const Context& context,
                    HighlightFlags flags,
                    DisplayBuffer& display_buffer)
    {
        m_callback(m_groups, context, flags, display_buffer);
    }

    HighlighterGroup& get_group(StringView path);
    HighlighterFunc get_highlighter(StringView path) const;

    Completions complete_id(StringView path, ByteCount cursor_pos) const;
    Completions complete_group_id(StringView path, ByteCount cursor_pos) const;

protected:
    Callback m_callback;
    GroupMap m_groups;
};

struct DefinedHighlighters : public HighlighterGroup,
                             public Singleton<DefinedHighlighters>
{
};

}

#endif // highlighter_group_hh_INCLUDED
