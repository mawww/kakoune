#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "dynamic_selection_list.hh"

namespace Kakoune
{

class Editor;
class Window;
class Buffer;
class Client;
class InputHandler;
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
    Context(InputHandler& input_handler, Editor& editor, String name = "");
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

    InputHandler& input_handler() const;
    bool has_input_handler() const { return (bool)m_input_handler; }

    UserInterface& ui() const;
    bool has_ui() const { return has_client(); }

    SelectionList& selections();
    const SelectionList& selections() const;
    std::vector<String>  selections_content() const;

    void change_editor(Editor& editor);

    void set_client(Client& client);

    OptionManager& options() const;
    HookManager& hooks() const;
    KeymapManager& keymaps() const;

    void print_status(DisplayLine status) const;

    void push_jump();
    const DynamicSelectionList& jump_forward();
    const DynamicSelectionList& jump_backward();
    void forget_jumps_to_buffer(Buffer& buffer);

    const String& name() const { return m_name; }
    void set_name(String name) { m_name = std::move(name); }

    bool is_editing() const { return m_edition_level!= 0; }
    void disable_undo_handling() { ++m_edition_level; }
private:
    void begin_edition();
    void end_edition();
    int m_edition_level = 0;

    friend struct ScopedEdition;

    safe_ptr<Editor>       m_editor;
    safe_ptr<InputHandler> m_input_handler;
    safe_ptr<Client>       m_client;

    String m_name;

    using JumpList = std::vector<DynamicSelectionList>;
    JumpList           m_jump_list;
    JumpList::iterator m_current_jump = m_jump_list.begin();
};

struct ScopedEdition
{
    ScopedEdition(Context& context)
        : m_context(context)
    { m_context.begin_edition(); }

    ~ScopedEdition()
    { m_context.end_edition(); }

    Context& context() const { return m_context; }
private:
    Context& m_context;
};

}
#endif // context_hh_INCLUDED
