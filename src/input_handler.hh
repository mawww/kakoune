#ifndef input_handler_hh_INCLUDED
#define input_handler_hh_INCLUDED

#include "completion.hh"
#include "constexpr_utils.hh"
#include "context.hh"
#include "face.hh"
#include "normal.hh"
#include "keys.hh"
#include "string.hh"
#include "utils.hh"
#include "safe_ptr.hh"
#include "display_buffer.hh"
#include "event_manager.hh"

namespace Kakoune
{

enum class MenuEvent
{
    Select,
    Abort,
    Validate
};
using MenuCallback = std::function<void (int, MenuEvent, Context&)>;

enum class PromptEvent
{
    Change,
    Abort,
    Validate
};
using PromptCallback = std::function<void (StringView, PromptEvent, Context&)>;
enum class PromptFlags
{
    None = 0,
    Password = 1 << 0,
    DropHistoryEntriesWithBlankPrefix = 1 << 1,
    Search = 1 << 2,
};
constexpr bool with_bit_ops(Meta::Type<PromptFlags>) { return true; }


using KeyCallback = std::function<void (Key, Context&)>;

class InputMode;
enum class KeymapMode : char;
enum class CursorMode;

using PromptCompleter = std::function<Completions (const Context&, CompletionFlags,
                                                   StringView, ByteCount)>;
enum class InsertMode : unsigned
{
    Insert,
    Append,
    Replace,
    InsertAtLineBegin,
    AppendAtLineEnd,
    OpenLineBelow,
    OpenLineAbove
};

class InputHandler : public SafeCountable
{
public:
    InputHandler(SelectionList selections,
                 Context::Flags flags = Context::Flags::None,
                 String name = "");
    ~InputHandler();

    // switch to insert mode
    void insert(InsertMode mode, int count);
    // repeat last insert mode key sequence
    void repeat_last_insert();

    // enter prompt mode, callback is called on each change,
    // abort or validation with corresponding PromptEvent value
    // returns to normal mode after validation if callback does
    // not change the mode itself
    void prompt(StringView prompt, String initstr, String emptystr,
                Face prompt_face, PromptFlags flags, char history_register,
                PromptCompleter completer, PromptCallback callback);
    void set_prompt_face(Face prompt_face);

    // enter menu mode, callback is called on each selection change,
    // abort or validation with corresponding MenuEvent value
    // returns to normal mode after validation if callback does
    // not change the mode itself
    void menu(Vector<DisplayLine> choices, MenuCallback callback);

    // execute callback on next keypress and returns to normal mode
    // if callback does not change the mode itself
    void on_next_key(StringView mode_name, KeymapMode mode, KeyCallback callback,
                     Timer::Callback idle_callback = Timer::Callback{});

    // process the given key
    void handle_key(Key key);

    void start_recording(char reg);
    bool is_recording() const;
    void stop_recording();
    char recording_reg() const { return m_recording_reg; }

    void reset_normal_mode();

    Context& context() { return m_context; }
    const Context& context() const { return m_context; }

    DisplayLine mode_line() const;

    std::pair<CursorMode, DisplayCoord> get_cursor_info() const;

    // Force an input handler into normal mode temporarily
    struct ScopedForceNormal
    {
        ScopedForceNormal(InputHandler& handler, NormalParams params);
        ~ScopedForceNormal();

    private:
        InputHandler& m_handler;
        InputMode* m_mode;
    };

private:
    Context m_context;

    friend class InputMode;
    Vector<RefPtr<InputMode>, MemoryDomain::Client> m_mode_stack;

    InputMode& current_mode() const { return *m_mode_stack.back(); }

    void push_mode(InputMode* new_mode);
    void pop_mode(InputMode* current_mode);

    struct Insertion{
        NestedBool recording;
        InsertMode mode;
        Vector<Key> keys;
        bool disable_hooks;
        int count;
    } m_last_insert = { {}, InsertMode::Insert, {}, false, 1 };

    char   m_recording_reg = 0;
    String m_recorded_keys;
    int    m_recording_level = -1;

    int    m_handle_key_level = 0;
};

enum class AutoInfo
{
    None = 0,
    Command = 1 << 0,
    OnKey   = 1 << 1,
    Normal  = 1 << 2
};

constexpr bool with_bit_ops(Meta::Type<AutoInfo>) { return true; }

constexpr auto enum_desc(Meta::Type<AutoInfo>)
{
    return make_array<EnumDesc<AutoInfo>>({
        { AutoInfo::Command, "command"},
        { AutoInfo::OnKey, "onkey"},
        { AutoInfo::Normal, "normal" }
    });
}

enum class AutoComplete
{
    None = 0,
    Insert = 0b01,
    Prompt = 0b10
};
constexpr bool with_bit_ops(Meta::Type<AutoComplete>) { return true; }

constexpr auto enum_desc(Meta::Type<AutoComplete>)
{
    return make_array<EnumDesc<AutoComplete>>({
        { AutoComplete::Insert, "insert"},
        { AutoComplete::Prompt, "prompt" }
    });
}

bool should_show_info(AutoInfo mask, const Context& context);
bool show_auto_info_ifn(StringView title, StringView info, AutoInfo mask, const Context& context);
void hide_auto_info_ifn(const Context& context, bool hide);

template<typename Cmd>
void on_next_key_with_autoinfo(const Context& context, StringView mode_name,
                               KeymapMode keymap_mode, Cmd cmd,
                               String title, String info)
{
    context.input_handler().on_next_key(mode_name,
        keymap_mode, [cmd](Key key, Context& context) mutable {
            bool hide = should_show_info(AutoInfo::OnKey, context);
            hide_auto_info_ifn(context, hide);
            cmd(key, context);
    }, [&context, title=std::move(title), info=std::move(info)](Timer&) {
           show_auto_info_ifn(title, info, AutoInfo::OnKey, context);
    });
}

void scroll_window(Context& context, LineCount offset, bool mouse_dragging = false);

}

#endif // input_handler_hh_INCLUDED
