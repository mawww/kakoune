#ifndef line_change_watcher_hh_INCLUDED
#define line_change_watcher_hh_INCLUDED

#include "buffer.hh"

namespace Kakoune
{

struct LineModification
{
    LineCount old_line; // line position in the old buffer
    LineCount new_line; // new line position
    LineCount num_removed; // number of lines removed after this one
    LineCount num_added; // number of lines added after this one

    LineCount diff() const { return new_line - old_line + num_added - num_removed; }
};

class LineChangeWatcher : public BufferChangeListener_AutoRegister
{
public:
    LineChangeWatcher (const Buffer& buffer)
        : BufferChangeListener_AutoRegister(const_cast<Buffer&>(buffer)) {}

    std::vector<LineModification> compute_modifications();
private:
    void on_insert(const Buffer& buffer, ByteCoord begin, ByteCoord end) override;
    void on_erase(const Buffer& buffer, ByteCoord begin, ByteCoord end) override;

    struct Change
    {
        LineCount pos;
        LineCount num;
    };
    std::vector<Change> m_changes;
};

}

#endif // line_change_watcher_hh_INCLUDED
