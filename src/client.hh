#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

#include "constexpr_utils.hh"
#include "display_buffer.hh"
#include "env_vars.hh"
#include "input_handler.hh"
#include "safe_ptr.hh"
#include "utils.hh"
#include "option_manager.hh"
#include "enum.hh"

namespace Kakoune
{

class Window;
class UserInterface;
class String;
struct Key;

enum class InfoStyle;
enum class MenuStyle;

class Client final : public SafeCountable, public OptionManagerWatcher
{
public:
    using OnExitCallback = std::function<void (int status)>;

    Client(std::unique_ptr<UserInterface>&& ui,
           std::unique_ptr<Window>&& window,
           SelectionList selections,
           int pid, EnvVarMap env_vars,
           String name,
           OnExitCallback on_exit);
    ~Client();

    Client(Client&&) = delete;

    bool is_ui_ok() const;

    bool process_pending_inputs();
    bool has_pending_inputs() const { return not m_pending_keys.empty(); }

    void menu_show(Vector<DisplayLine> choices, BufferCoord anchor, MenuStyle style);
    void menu_select(int selected);
    void menu_hide();

    void info_show(DisplayLine title, DisplayLineList content, BufferCoord anchor, InfoStyle style);
    void info_show(StringView title, StringView content, BufferCoord anchor, InfoStyle style);
    void info_hide(bool even_modal = false);

    void print_status(DisplayLine status_line);
    const DisplayLine& current_status() const { return m_status_line; }

    DisplayCoord dimensions() const;

    void force_redraw();
    void redraw_ifn();

    void check_if_buffer_needs_reloading();

    Context& context() { return m_input_handler.context(); }
    const Context& context() const { return m_input_handler.context(); }

    InputHandler& input_handler() { return m_input_handler; }
    const InputHandler& input_handler() const { return m_input_handler; }

    void change_buffer(Buffer& buffer);

    StringView get_env_var(StringView name) const;

    void exit(int status) { m_on_exit(status); }

    int pid() const { return m_pid; }

private:
    void on_option_changed(const Option& option) override;

    void on_buffer_reload_key(Key key);
    void close_buffer_reload_dialog();
    void reload_buffer();

    DisplayLine generate_mode_line() const;

    std::unique_ptr<UserInterface> m_ui;
    std::unique_ptr<Window> m_window;

    const int m_pid;

    OnExitCallback m_on_exit;

    EnvVarMap m_env_vars;

    InputHandler m_input_handler;

    DisplayLine m_status_line;
    DisplayLine m_mode_line;

    enum PendingUI : int
    {
        MenuShow   = 1 << 0,
        MenuSelect = 1 << 1,
        MenuHide   = 1 << 2,
        InfoShow   = 1 << 3,
        InfoHide   = 1 << 4,
        StatusLine = 1 << 5,
        Draw       = 1 << 6,
        Refresh    = 1 << 7,
    };
    int m_ui_pending = 0;

    struct Menu
    {
        Vector<DisplayLine> items;
        BufferCoord anchor;
        Optional<DisplayCoord> ui_anchor;
        MenuStyle style;
        int selected;
    } m_menu{};

    struct Info
    {
        DisplayLine title;
        DisplayLineList content;
        BufferCoord anchor;
        Optional<DisplayCoord> ui_anchor;
        InfoStyle style;
    } m_info{};

    Vector<Key, MemoryDomain::Client> m_pending_keys;

    bool m_buffer_reload_dialog_opened = false;
};

enum class Autoreload
{
    Yes,
    No,
    Ask
};

constexpr auto enum_desc(Meta::Type<Autoreload>)
{
    return make_array<EnumDesc<Autoreload>>({
        { Autoreload::Yes, "yes" },
        { Autoreload::No, "no" },
        { Autoreload::Ask, "ask" },
        { Autoreload::Yes, "true" },
        { Autoreload::No, "false" }
    });
}

}

#endif // client_hh_INCLUDED
