#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

#include "keys.hh"
#include "completion.hh"
#include "utils.hh"
#include "string.hh"
#include "window.hh"
#include "user_interface.hh"

namespace Kakoune
{

class Editor;
class Window;
class Context;

using MenuCallback = std::function<void (int, Context&)>;
using PromptCallback = std::function<void (const String&, Context&)>;
using KeyCallback = std::function<void (const Key&, Context&)>;

class Client : public SafeCountable
{
public:
    Client(UserInterface* ui);
    ~Client();

    void insert(Editor& editor, IncrementalInserter::Mode mode);
    void repeat_last_insert(Editor& editor, Context& context);

    void prompt(const String& prompt, Completer completer,
                PromptCallback callback);

    void menu(const memoryview<String>& choices,
              MenuCallback callback);

    void print_status(const String& status, CharCount cursor_pos = -1);
    void draw_window(Window& window);

    void on_next_key(KeyCallback callback);

    void handle_next_input(Context& context);

private:
    void reset_normal_mode();
    std::pair<IncrementalInserter::Mode, std::vector<Key>> m_last_insert;

    std::unique_ptr<UserInterface> m_ui;

    class Mode;
    std::unique_ptr<Mode> m_mode;

    class NormalMode;
    class MenuMode;
    class PromptMode;
    class NextKeyMode;
    class InsertMode;
};

struct prompt_aborted {};

}

#endif // client_hh_INCLUDED
