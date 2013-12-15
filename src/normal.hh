#ifndef normal_hh_INCLUDED
#define normal_hh_INCLUDED

#include "keys.hh"

#include <functional>
#include <unordered_map>

namespace Kakoune
{

class Context;

enum class InsertMode : unsigned
{
    Insert,
    Append,
    Replace,
    InsertAtLineBegin,
    InsertAtNextLineBegin,
    AppendAtLineEnd,
    OpenLineBelow,
    OpenLineAbove
};

using KeyMap = std::unordered_map<Key, std::function<void (Context& context, int param)>>;
extern KeyMap keymap;

}

#endif // normal_hh_INCLUDED
