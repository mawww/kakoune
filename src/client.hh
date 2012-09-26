#ifndef client_hh_INCLUDED
#define client_hh_INCLUDED

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

class ClientMode;
enum class InsertMode : unsigned;

class Client : public SafeCountable
{
public:
    Client();
    ~Client();

    void insert(Editor& editor, InsertMode mode);
    void repeat_last_insert(Context& context);

    void prompt(const String& prompt, Completer completer,
                PromptCallback callback, Context& context);

    void menu(const memoryview<String>& choices,
              MenuCallback callback, Context& context);

    void on_next_key(KeyCallback callback);

    void handle_next_input(Context& context);

private:
    friend class ClientMode;
    std::unique_ptr<ClientMode> m_mode;
    std::pair<InsertMode, std::vector<Key>> m_last_insert;
};

struct prompt_aborted {};

}

#endif // client_hh_INCLUDED
