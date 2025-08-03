#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

#include "array.hh"
#include "clock.hh"
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
    using OnExitCallback = Function<void (int status)>;

    Client(UniquePtr<UserInterface>&& ui,
           UniquePtr<Window>&& window,
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
    bool info_pending() const { return m_ui_pending & PendingUI::InfoShow; };
    bool status_line_pending() const { return m_ui_pending & PendingUI::StatusLine; };

    void print_status(DisplayLine status_line);
    const DisplayLine& current_status() const { return m_status_line; }

    DisplayCoord dimensions() const;

    void schedule_clear();
    void clear_pending();

    void force_redraw(bool full = false);
    void redraw_ifn();

    void check_if_buffer_needs_reloading();

    Context& context() { return m_input_handler.context(); }
    const Context& context() const { return m_input_handler.context(); }

    InputHandler& input_handler() { return m_input_handler; }
    const InputHandler& input_handler() const { return m_input_handler; }

    void change_buffer(Buffer& buffer, Optional<FunctionRef<void()>> set_selection);

    StringView get_env_var(StringView name) const;

    void exit(int status) { m_on_exit(status); }

    int pid() const { return m_pid; }

private:
    void on_option_changed(const Option& option) override;

    void on_buffer_reload_key(Key key);
    void close_buffer_reload_dialog();
    void reload_buffer();

    DisplayLine generate_mode_line() const;

    UniquePtr<UserInterface> m_ui;
    UniquePtr<Window> m_window;

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

    enum class PendingClear
    {
        None = 0,
        Info = 0b01,
        StatusLine = 0b10
    };
    friend constexpr bool with_bit_ops(Meta::Type<PendingClear>) { return true; }
    PendingClear m_pending_clear = PendingClear::None;


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

class BusyIndicator
{
public:
    BusyIndicator(const Context& context,
                  Function<DisplayLine(std::chrono::seconds)> status_message,
                  TimePoint wait_time = Clock::now());
    ~BusyIndicator();
private:
    const Context& m_context;
    Timer m_timer;
    Optional<DisplayLine> m_previous_status;
};

}

#endif // client_hh_INCLUDED
