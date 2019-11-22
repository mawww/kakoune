#ifndef user_interface_hh_INCLUDED
#define user_interface_hh_INCLUDED

#include "array_view.hh"
#include "hash_map.hh"

#include <functional>

namespace Kakoune
{

class String;
class DisplayBuffer;
class DisplayLine;
using DisplayLineList = Vector<DisplayLine, MemoryDomain::Display>;
struct DisplayCoord;
struct Face;
struct Key;

enum class MenuStyle
{
    Prompt,
    Search,
    Inline
};

enum class InfoStyle
{
    Prompt,
    Inline,
    InlineAbove,
    InlineBelow,
    MenuDoc,
    Modal
};

enum class EventMode;

enum class CursorMode
{
    Prompt,
    Buffer,
};

using OnKeyCallback = std::function<void(Key key)>;

class UserInterface
{
public:
    virtual ~UserInterface() = default;

    virtual bool is_ok() const = 0;

    virtual void menu_show(ConstArrayView<DisplayLine> choices,
                           DisplayCoord anchor, Face fg, Face bg,
                           MenuStyle style) = 0;
    virtual void menu_select(int selected) = 0;
    virtual void menu_hide() = 0;

    virtual void info_show(const DisplayLine& title,
                           const DisplayLineList& content,
                           DisplayCoord anchor, Face face,
                           InfoStyle style) = 0;
    virtual void info_hide() = 0;

    virtual void draw(const DisplayBuffer& display_buffer,
                      const Face& default_face,
                      const Face& padding_face) = 0;

    virtual void draw_status(const DisplayLine& status_line,
                             const DisplayLine& mode_line,
                             const Face& default_face) = 0;

    virtual DisplayCoord dimensions() = 0;

    virtual void set_cursor(CursorMode mode, DisplayCoord coord) = 0;

    virtual void refresh(bool force) = 0;

    virtual void set_on_key(OnKeyCallback callback) = 0;

    using Options = HashMap<String, String, MemoryDomain::Options>;
    virtual void set_ui_options(const Options& options) = 0;
};

}

#endif // user_interface_hh_INCLUDED
