#ifndef input_handler_hh_INCLUDED
#define input_handler_hh_INCLUDED

#include "completion.hh"
#include "context.hh"
#include "face.hh"
#include "normal.hh"
#include "keys.hh"
#include "string.hh"
#include "utils.hh"
#include "safe_ptr.hh"

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
using KeyCallback = std::function<void (Key, Context&)>;

class InputMode;
class DisplayLine;
enum class InsertMode : unsigned;
enum class KeymapMode : int;

class InputHandler : public SafeCountable
{
public:
    InputHandler(SelectionList selections,
                 Context::Flags flags = Context::Flags::None,
                 String name = "");
    ~InputHandler();

    // switch to insert mode
    void insert(InsertMode mode);
    // repeat last insert mode key sequence
    void repeat_last_insert();

    // enter prompt mode, callback is called on each change,
    // abort or validation with corresponding PromptEvent value
    // returns to normal mode after validation if callback does
    // not change the mode itself
    void prompt(StringView prompt, String initstr,
                Face prompt_face, Completer completer,
                PromptCallback callback);
    void set_prompt_face(Face prompt_face);

    // enter menu mode, callback is called on each selection change,
    // abort or validation with corresponding MenuEvent value
    // returns to normal mode after validation if callback does
    // not change the mode itself
    void menu(ArrayView<const String> choices, MenuCallback callback);

    // execute callback on next keypress and returns to normal mode
    // if callback does not change the mode itself
    void on_next_key(KeymapMode mode, KeyCallback callback);

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
    void clear_mode_trash();

private:
    Context m_context;

    friend class InputMode;
    std::unique_ptr<InputMode> m_mode;
    Vector<std::unique_ptr<InputMode>> m_mode_trash;

    void change_input_mode(InputMode* new_mode);

    using Insertion = std::pair<InsertMode, Vector<Key>>;
    Insertion m_last_insert = {InsertMode::Insert, {}};

    char   m_recording_reg = 0;
    String m_recorded_keys;

    int    m_handle_key_level = 0;
};

}

#endif // input_handler_hh_INCLUDED
