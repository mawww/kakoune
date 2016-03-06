#ifndef json_ui_hh_INCLUDED
#define json_ui_hh_INCLUDED

#include "user_interface.hh"
#include "event_manager.hh"
#include "coord.hh"

namespace Kakoune
{

class Value;

class JsonUI : public UserInterface
{
public:
    JsonUI();

    JsonUI(const JsonUI&) = delete;
    JsonUI& operator=(const JsonUI&) = delete;

    void draw(const DisplayBuffer& display_buffer,
              const Face& default_face) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    bool is_key_available() override;
    Key  get_key() override;

    void menu_show(ConstArrayView<DisplayLine> items,
                   CharCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(StringView title, StringView content,
                   CharCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void refresh() override;

    void set_input_callback(InputCallback callback) override;

    void set_ui_options(const Options& options) override;

    CharCoord dimensions() override;

private:
    void parse_requests(EventMode mode);
    void eval_json(const Value& value);

    InputCallback m_input_callback;
    FDWatcher m_stdin_watcher;
    Vector<Key> m_pending_keys;
    CharCoord m_dimensions;
    String m_incoming_text;
};

}

#endif // json_ui_hh_INCLUDED
