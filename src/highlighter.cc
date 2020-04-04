#include "highlighter.hh"

#include "buffer_utils.hh"

namespace Kakoune
{

void Highlighter::highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range)
{
    if (context.pass & m_passes) try
    {
        do_highlight(context, display_buffer, range);
    }
    catch (runtime_error& error)
    {
        write_to_debug_buffer(format("Error while highlighting: {}", error.what()));
    }
}

void Highlighter::compute_display_setup(HighlightContext context, DisplaySetup& setup) const
{
    if (context.pass & m_passes)
        do_compute_display_setup(context, setup);
}


bool Highlighter::has_children() const
{
    return false;
}

Highlighter& Highlighter::get_child(StringView path)
{
    throw runtime_error("this highlighter does not hold children");
}

void Highlighter::add_child(String, std::unique_ptr<Highlighter>&&, bool)
{
    throw runtime_error("this highlighter does not hold children");
}

void Highlighter::remove_child(StringView id)
{
    throw runtime_error("this highlighter does not hold children");
}

Completions Highlighter::complete_child(StringView path, ByteCount cursor_pos, bool group) const
{
    throw runtime_error("this highlighter does not hold children");
}

void Highlighter::fill_unique_ids(Vector<StringView>& unique_ids) const
{}

}
