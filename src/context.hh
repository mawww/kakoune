#ifndef context_hh_INCLUDED
#define context_hh_INCLUDED

#include "selection.hh"
#include "optional.hh"
#include "utils.hh"

#include <functional>

namespace Kakoune
{

class Context;
class Window;
class Buffer;
class Client;
class Scope;
class InputHandler;
class DisplayLine;

class AliasRegistry;
class FaceRegistry;
class OptionManager;
class KeymapManager;
class HookManager;

enum Direction { Backward = -1, Forward = 1 };
enum Distance { FullScreen = 0, HalfScreen = 1, Line = 2 };

struct JumpList
{
    void push(SelectionList jump, Optional<size_t> index = {});
    const SelectionList& forward(Context& context, int count);
    const SelectionList& backward(Context& context, int count);
    void forget_buffer(Buffer& buffer);

    friend bool operator==(const JumpList& lhs, const JumpList& rhs) = default;

    size_t current_index() const { return m_current; }

    ConstArrayView<SelectionList> get_as_list() const { return m_jumps; }

private:
    using Contents = Vector<SelectionList, MemoryDomain::Selections>;
    Contents m_jumps;
    size_t m_current = 0;
};

using LastSelectFunc = std::function<void (Context&)>;

struct LocalScope;

// A Context is used to access non singleton objects for various services
// in commands.
//
// The Context object links a Client, a Window, an InputHandler and a
// SelectionList. It may represent an interactive user window, a hook
// execution or a macro replay context.
class Context
{
public:
    enum class Flags
    {
        None  = 0,
        Draft = 1,
    };
    friend constexpr bool with_bit_ops(Meta::Type<Flags>) { return true; }

    Context(InputHandler& input_handler, SelectionList selections,
            Flags flags, String name = "");

    struct EmptyContextFlag {};
    explicit Context(EmptyContextFlag);
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    Buffer& buffer() const;
    bool has_buffer() const { return not m_selection_history.empty(); }

    Window& window() const;
    bool has_window() const { return (bool)m_window; }

    Client& client() const;
    bool has_client() const { return (bool)m_client; }

    InputHandler& input_handler() const;
    bool has_input_handler() const { return (bool)m_input_handler; }

    SelectionList& selections(bool update = true);
    const SelectionList& selections(bool update = true) const;
    Vector<String>  selections_content() const;

    // Return potentially out of date selections
    SelectionList& selections_write_only();

    void end_selection_edition() { m_selection_history.end_edition(); }
    template<Direction direction>
    void undo_selection_change();

    void change_buffer(Buffer& buffer, Optional<FunctionRef<void()>> set_selection = {});
    void forget_buffer(Buffer& buffer);

    void set_client(Client& client);
    void set_window(Window& window);

    friend struct LocalScope;

    Scope& scope(bool allow_local = true) const;
    Scope* local_scope() const { return m_local_scopes.empty() ? nullptr : m_local_scopes.back(); }

    OptionManager& options() const;
    HookManager&   hooks()   const;
    KeymapManager& keymaps() const;
    AliasRegistry& aliases() const;
    FaceRegistry&  faces(bool allow_local = true) const;

    void print_status(DisplayLine status) const;

    StringView main_sel_register_value(StringView reg) const;

    const String& name() const { return m_name; }
    void set_name(String name);

    bool is_editing() const { return m_edition_level!= 0; }
    void disable_undo_handling() { m_edition_level = -1; }

    NestedBool& hooks_disabled() { return m_hooks_disabled; }
    const NestedBool& hooks_disabled() const { return m_hooks_disabled; }

    NestedBool& keymaps_disabled() { return m_keymaps_disabled; }
    const NestedBool& keymaps_disabled() const { return m_keymaps_disabled; }

    Flags flags() const { return m_flags; }

    JumpList& jump_list() { return m_jump_list; }
    void push_jump(bool force = false);

    template<typename Func>
    void set_last_select(Func&& last_select) { m_last_select = std::forward<Func>(last_select); }

    void repeat_last_select() { if (m_last_select) m_last_select(*this); }

    Buffer* last_buffer() const;

    bool ensure_cursor_visible = true;
private:
    void begin_edition();
    void end_edition();
    int m_edition_level = 0;
    size_t m_edition_timestamp = 0;

    friend struct ScopedEdition;
    friend struct ScopedSelectionEdition;

    Flags m_flags = Flags::None;

    SafePtr<InputHandler> m_input_handler;
    SafePtr<Window>       m_window;
    SafePtr<Client>       m_client;
    std::vector<Scope*>   m_local_scopes;

    class SelectionHistory {
    public:
        SelectionHistory(Context& context);
        SelectionHistory(Context& context, SelectionList selections);
        void initialize(SelectionList selections);
        bool empty() const { return m_history.empty() and not m_staging; }
        SelectionList& selections(bool update = true);

        void begin_edition();
        void end_edition();
        bool in_edition() const { return m_in_edition; }

        template<Direction direction>
        void undo();
        void forget_buffer(Buffer& buffer);
    private:
        enum class HistoryId : size_t { First = 0, Invalid = (size_t)-1 };

        struct HistoryNode
        {
            HistoryNode(SelectionList selections, HistoryId parent) : selections(selections), parent(parent) {}

            SelectionList selections;
            HistoryId parent;
            HistoryId redo_child = HistoryId::Invalid;
        };

              HistoryId next_history_id() const noexcept    { return (HistoryId)m_history.size(); }
              HistoryNode& history_node(HistoryId id)       { return m_history[(size_t)id]; }
        const HistoryNode& history_node(HistoryId id) const { return m_history[(size_t)id]; }
              HistoryNode& current_history_node()           { kak_assert((size_t)m_history_id < m_history.size()); return m_history[(size_t)m_history_id]; }

        Context&              m_context;
        Vector<HistoryNode>   m_history;
        HistoryId             m_history_id = HistoryId::Invalid;
        Optional<HistoryNode> m_staging;
        NestedBool            m_in_edition;
    };
    SelectionHistory m_selection_history;

    String m_name;

    JumpList m_jump_list;

    LastSelectFunc m_last_select;

    NestedBool m_hooks_disabled;
    NestedBool m_keymaps_disabled;
};

struct ScopedEdition
{
    ScopedEdition(Context& context);
    ~ScopedEdition();
    ScopedEdition(const ScopedEdition&) = delete;

    Context& context() const { return m_context; }
private:
    Context& m_context;
    SafePtr<Buffer> m_buffer;
};

struct ScopedSelectionEdition
{
    ScopedSelectionEdition(Context& context);
    ScopedSelectionEdition(ScopedSelectionEdition&& other);
    ~ScopedSelectionEdition();

private:
    Context& m_context;
    SafePtr<Buffer> m_buffer;
};

}
#endif // context_hh_INCLUDED
