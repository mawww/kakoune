#ifndef json_ui_hh_INCLUDED
#define json_ui_hh_INCLUDED

#include "user_interface.hh"
#include "event_manager.hh"
#include "coord.hh"
#include "string.hh"

namespace Kakoune
{

struct Value;

class JsonUI : public UserInterface
{
public:
    JsonUI();

    JsonUI(const JsonUI&) = delete;
    JsonUI& operator=(const JsonUI&) = delete;

    bool is_ok() const override { return m_stdin_watcher.fd() != -1; }

    void draw(const DisplayBuffer& display_buffer,
              const Face& default_face,
              const Face& buffer_padding) override;

    void draw_status(const DisplayLine& status_line,
                     const DisplayLine& mode_line,
                     const Face& default_face) override;

    void menu_show(ConstArrayView<DisplayLine> items,
                   DisplayCoord anchor, Face fg, Face bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const DisplayLine& title, const DisplayLineList& content,
                   DisplayCoord anchor, Face face,
                   InfoStyle style) override;
    void info_hide() override;

    void set_cursor(CursorMode mode, DisplayCoord coord) override;

    void refresh(bool force) override;

    DisplayCoord dimensions() override;
    void set_on_key(OnKeyCallback callback) override;
    void set_ui_options(const Options& options) override;

private:
    void parse_requests(EventMode mode);
    void eval_json(const Value& value);

    FDWatcher m_stdin_watcher;
    OnKeyCallback m_on_key;
    Vector<Key, MemoryDomain::Client> m_pending_keys;
    DisplayCoord m_dimensions;
    String m_requests;
};

}

#endif // json_ui_hh_INCLUDED
