#ifndef user_interface_hh_INCLUDED
#define user_interface_hh_INCLUDED

#include "array_view.hh"
#include "id_map.hh"

#include <functional>

namespace Kakoune
{

class String;
class DisplayBuffer;
class DisplayLine;
struct DisplayCoord;
struct Face;
struct Key;

enum class MenuStyle
{
    Prompt,
    Inline
};

enum class InfoStyle
{
    Prompt,
    Inline,
    InlineAbove,
    InlineBelow,
    MenuDoc
};

enum class EventMode;

using InputCallback = std::function<void(EventMode mode)>;

class UserInterface
{
public:
    virtual ~UserInterface() {}

    virtual void menu_show(ConstArrayView<DisplayLine> choices,
                           DisplayCoord anchor, Face fg, Face bg,
                           MenuStyle style) = 0;
    virtual void menu_select(int selected) = 0;
    virtual void menu_hide() = 0;

    virtual void info_show(StringView title, StringView content,
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
    virtual bool is_key_available() = 0;
    virtual Key  get_key() = 0;

    virtual void refresh(bool force) = 0;

    virtual void set_input_callback(InputCallback callback) = 0;

    using Options = IdMap<String, MemoryDomain::Options>;
    virtual void set_ui_options(const Options& options) = 0;
};

}

#endif // user_interface_hh_INCLUDED
