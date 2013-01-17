#ifndef input_handler_hh_INCLUDED
#define input_handler_hh_INCLUDED

#include "keys.hh"
#include "completion.hh"
#include "utils.hh"
#include "string.hh"

namespace Kakoune
{

class Editor;
class Context;

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
using PromptCallback = std::function<void (const String&, PromptEvent, Context&)>;
using KeyCallback = std::function<void (const Key&, Context&)>;

class InputMode;
enum class InsertMode : unsigned;

class InputHandler : public SafeCountable
{
public:
    InputHandler();
    ~InputHandler();

    // switch to insert mode
    void insert(Context& context, InsertMode mode);
    // repeat last insert mode key sequence
    void repeat_last_insert(Context& context);

    // enter prompt mode, callback is called on each change,
    // abort or validation with corresponding PromptEvent value
    // returns to normal mode after validation if callback does
    // not change the mode itself
    void prompt(const String& prompt, Completer completer,
                PromptCallback callback, Context& context);

    // enter menu mode, callback is called on each selection change,
    // abort or validation with corresponding MenuEvent value
    // returns to normal mode after validation if callback does
    // not change the mode itself
    void menu(const memoryview<String>& choices,
              MenuCallback callback, Context& context);

    // execute callback on next keypress and returns to normal mode
    // if callback does not change the mode itself
    void on_next_key(KeyCallback callback);

    // read and process all inputs available in context
    // user interface
    void handle_available_inputs(Context& context);

private:
    friend class InputMode;
    std::unique_ptr<InputMode> m_mode;
    std::vector<std::unique_ptr<InputMode>> m_mode_trash;
};

struct prompt_aborted {};

}

#endif // input_handler_hh_INCLUDED
