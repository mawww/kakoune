#ifndef user_interface_hh_INCLUDED
#define user_interface_hh_INCLUDED

#include "memoryview.hh"
#include "keys.hh"
#include "units.hh"
#include "utils.hh"
#include "color.hh"

namespace Kakoune
{

class String;
class DisplayBuffer;
struct DisplayCoord;

enum class MenuStyle
{
    Prompt,
    Inline
};

using InputCallback = std::function<void()>;

class UserInterface : public SafeCountable
{
public:
    virtual ~UserInterface() {}
    virtual void print_status(const String& status, CharCount cursor_pos = -1) = 0;

    virtual void menu_show(const memoryview<String>& choices,
                           DisplayCoord anchor, ColorPair fg, ColorPair bg,
                           MenuStyle style) = 0;
    virtual void menu_select(int selected) = 0;
    virtual void menu_hide() = 0;

    virtual void info_show(const String& content,
                           const DisplayCoord& anchor, MenuStyle style) = 0;
    virtual void info_hide() = 0;

    virtual void draw(const DisplayBuffer& display_buffer,
                      const String& mode_line) = 0;
    virtual DisplayCoord dimensions() = 0;
    virtual bool is_key_available() = 0;
    virtual Key  get_key() = 0;

    virtual void set_input_callback(InputCallback callback) = 0;
};

}

#endif // user_interface_hh_INCLUDED
