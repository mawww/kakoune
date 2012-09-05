#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

#include "keys.hh"
#include "completion.hh"
#include "utils.hh"
#include "string.hh"
#include "editor.hh"

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
    Client();
    virtual ~Client() {}
    virtual void draw_window(Window& window) = 0;
    virtual void print_status(const String& status,
                              CharCount cursor_pos = -1) = 0;

    void insert(Editor& editor, IncrementalInserter::Mode mode);
    void repeat_last_insert(Editor& editor, Context& context);

    void prompt(const String& prompt, Completer completer,
                PromptCallback callback);

    void menu(const memoryview<String>& choices,
              MenuCallback callback);

    void on_next_key(KeyCallback callback);

    void handle_next_input(Context& context);

private:
    virtual void   menu_show(const memoryview<String>& choices) = 0;
    virtual void   menu_select(int selected) = 0;
    virtual void   menu_hide() = 0;
    virtual Key    get_key() = 0;

    void reset_normal_mode();
    std::pair<IncrementalInserter::Mode, std::vector<Key>> m_last_insert;

    class Mode
    {
    public:
        Mode(Client& client) : m_client(client) {}
        virtual ~Mode() {}
        Mode(const Mode&) = delete;
        Mode& operator=(const Mode&) = delete;

        virtual void on_key(const Key& key, Context& context) = 0;
    protected:
        Client& m_client;
    };
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
