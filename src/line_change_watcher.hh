#ifndef line_change_watcher_hh_INCLUDED
#define line_change_watcher_hh_INCLUDED

#include "units.hh"
#include "utils.hh"

namespace Kakoune
{

class Buffer;

struct LineModification
{
    LineCount old_line; // line position in the old buffer
    LineCount new_line; // new line position
    LineCount num_removed; // number of lines removed after this one
    LineCount num_added; // number of lines added after this one

    LineCount diff() const { return new_line - old_line + num_added - num_removed; }
};

class LineChangeWatcher
{
public:
    LineChangeWatcher (const Buffer& buffer);

    std::vector<LineModification> compute_modifications();

    const Buffer& buffer() const { return *m_buffer; }
private:
    safe_ptr<const Buffer> m_buffer;
    size_t m_timestamp;
};

}

#endif // line_change_watcher_hh_INCLUDED
