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

using MenuCallback = std::function<void (int, Context&)>;
using PromptCallback = std::function<void (const String&, Context&)>;
using KeyCallback = std::function<void (const Key&, Context&)>;

class InputMode;
enum class InsertMode : unsigned;

class InputHandler : public SafeCountable
{
public:
    InputHandler();
    ~InputHandler();

    void insert(Context& context, InsertMode mode);
    void repeat_last_insert(Context& context);

    void prompt(const String& prompt, Completer completer,
                PromptCallback callback, Context& context);

    void menu(const memoryview<String>& choices,
              MenuCallback callback, Context& context);

    void on_next_key(KeyCallback callback);

    void handle_available_inputs(Context& context);

private:
    friend class InputMode;
    std::unique_ptr<InputMode> m_mode;
    std::vector<std::unique_ptr<InputMode>> m_mode_trash;
};

struct prompt_aborted {};

}

#endif // input_handler_hh_INCLUDED
