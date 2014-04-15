#ifndef ncurses_hh_INCLUDED
#define ncurses_hh_INCLUDED

#include "display_buffer.hh"
#include "event_manager.hh"
#include "user_interface.hh"

namespace Kakoune
{

struct NCursesWin;

class NCursesUI : public UserInterface
{
public:
    NCursesUI();
    ~NCursesUI();

    NCursesUI(const NCursesUI&) = delete;
    NCursesUI& operator=(const NCursesUI&) = delete;

    void draw(const DisplayBuffer& display_buffer,
              const DisplayLine& status_line,
              const DisplayLine& mode_line) override;

    bool   is_key_available() override;
    Key    get_key() override;

    void menu_show(memoryview<String> items,
                   DisplayCoord anchor, ColorPair fg, ColorPair bg,
                   MenuStyle style) override;
    void menu_select(int selected) override;
    void menu_hide() override;

    void info_show(const String& title, const String& content,
                   DisplayCoord anchor, ColorPair colors,
                   MenuStyle style) override;
    void info_hide() override;

    void refresh() override;

    void set_input_callback(InputCallback callback) override;

    DisplayCoord dimensions() override;

    static void abort();
private:
    friend void on_term_resize(int);
    void redraw();
    void draw_line(const DisplayLine& line, CharCount col_index) const;

    DisplayCoord m_dimensions;
    void update_dimensions();

    NCursesWin* m_menu_win = nullptr;
    std::vector<String> m_items;
    ColorPair m_menu_fg;
    ColorPair m_menu_bg;
    int m_selected_item = 0;
    int m_menu_columns = 1;
    LineCount m_menu_top_line = 0;
    void draw_menu();

    NCursesWin* m_info_win = nullptr;

    FDWatcher     m_stdin_watcher;
    InputCallback m_input_callback;

    bool m_dirty = false;
};

}

#endif // ncurses_hh_INCLUDED

