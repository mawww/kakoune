#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "dynamic_selection_list.hh"

namespace Kakoune
{

class Editor;
class Window;
class Buffer;
class Client;
class UserInterface;
class DisplayLine;
class KeymapManager;

// A Context is used to access non singleton objects for various services
// in commands.
//
// The Context object links an Client, an Editor (which may be a Window),
// and a UserInterface. It may represent an interactive user window, or
// a hook execution or a macro replay.
class Context
{
public:
    Context();
    explicit Context(Editor& editor);
    Context(Client& client, Editor& editor);
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Buffer& buffer() const;
    bool has_buffer() const { return (bool)m_editor; }

    Editor& editor() const;
    bool has_editor() const { return (bool)m_editor; }

    Window& window() const;
    bool has_window() const;

    Client& client() const;
    bool has_client() const { return (bool)m_client; }

    UserInterface& ui() const;
    bool has_ui() const { return (bool)m_client; }

    void change_editor(Editor& editor);

    OptionManager& options() const;
    HookManager& hooks() const;
    KeymapManager& keymaps() const;

    void print_status(DisplayLine status) const;

    void push_jump();
    const DynamicSelectionList& jump_forward();
    const DynamicSelectionList& jump_backward();
    void forget_jumps_to_buffer(Buffer& buffer);

private:
    safe_ptr<Editor>  m_editor;
    safe_ptr<Client>  m_client;

    using JumpList = std::vector<DynamicSelectionList>;
    JumpList           m_jump_list;
    JumpList::iterator m_current_jump = m_jump_list.begin();
};

}
#endif // context_hh_INCLUDED
