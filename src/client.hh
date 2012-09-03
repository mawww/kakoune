#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

#include "keys.hh"
#include "completion.hh"
#include "utils.hh"
#include "string.hh"

namespace Kakoune
{

class Editor;
class Window;
class Context;

enum class MenuCommand
{
    SelectFirst,
    SelectPrev,
    SelectNext,
    Close,
};

using MenuCallback = std::function<void (int, Context&)>;
using PromptCallback = std::function<void (const String&, Context&)>;

class Client : public SafeCountable
{
public:
    Client();
    virtual ~Client() {}
    virtual void draw_window(Window& window) = 0;
    virtual void print_status(const String& status,
                              CharCount cursor_pos = -1) = 0;

    void prompt(const String& prompt, Completer completer,
                PromptCallback callback);

    void menu(const memoryview<String>& choices,
              MenuCallback callback);

    void handle_next_input(Context& context);
    virtual Key    get_key() = 0;

private:

    virtual void   show_menu(const memoryview<String>& choices) = 0;
    virtual void   menu_ctrl(MenuCommand command) = 0;


    void reset_normal_mode();
    class Mode
    {
    public:
        Mode(Client& client) : m_client(client) {}
        virtual ~Mode() {}
        virtual void on_key(const Key& key, Context& context) = 0;
    protected:
        Client& m_client;
    };
    std::unique_ptr<Mode> m_mode;

    class NormalMode;
    class MenuMode;
    class PromptMode;
};

struct prompt_aborted {};

}

#endif // client_hh_INCLUDED
