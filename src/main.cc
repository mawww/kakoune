#include "window.hh"
#include "buffer.hh"
#include "shell_manager.hh"
#include "commands.hh"
#include "command_manager.hh"
#include "buffer_manager.hh"
#include "register_manager.hh"
#include "selectors.hh"
#include "assert.hh"
#include "debug.hh"
#include "highlighters.hh"
#include "highlighter_registry.hh"
#include "filters.hh"
#include "filter_registry.hh"
#include "hook_manager.hh"
#include "option_manager.hh"
#include "context.hh"
#include "ncurses.hh"
#include "regex.hh"

#include <unordered_map>
#include <sys/types.h>
#include <sys/wait.h>

using namespace Kakoune;
using namespace std::placeholders;

namespace Kakoune
{

Context main_context;
bool quit_requested = false;

struct InsertSequence
{
    IncrementalInserter::Mode mode;
    std::vector<Key>          keys;

    InsertSequence() : mode(IncrementalInserter::Mode::Insert) {}
};

InsertSequence last_insert_sequence;

template<typename GetKey, typename Redraw>
void insert_sequence(IncrementalInserter& inserter,
                     GetKey get_key, Redraw redraw)
{
    while (true)
    {
        Key key = get_key();
        switch (key.modifiers)
        {
        case Key::Modifiers::None:
            switch (key.key)
            {
            case 27:
                return;
            default:
                inserter.insert(String() + key.key);
            }
            break;
        case Key::Modifiers::Control:
            switch (key.key)
            {
            case 'r':
            {
                Key next_key = get_key();
                if (next_key.modifiers == Key::Modifiers::None)
                    inserter.insert(RegisterManager::instance()[next_key.key]);
                break;
            }
            case 'm':
                inserter.insert(String() + '\n');
                break;
            case 'i':
                inserter.insert(String() + '\t');
                break;
            case 'd':
                inserter.move_cursors({0, -1});
                break;
            case 'e':
                inserter.move_cursors({0,  1});
                break;
            case 'g':
                inserter.erase();
                break;
            }
            break;
        }
        redraw();
    }
}

template<IncrementalInserter::Mode mode>
void do_insert(const Context& context)
{
    last_insert_sequence.mode = mode;
    last_insert_sequence.keys.clear();
    IncrementalInserter inserter(context.editor(), mode);
    draw_editor_ifn(context.editor());
    insert_sequence(inserter,
                    [&]() { Key key = get_key();
                            last_insert_sequence.keys.push_back(key);
                            return key; },
                    [&]() { draw_editor_ifn(context.editor()); });
}

void do_repeat_insert(const Context& context)
{
    if (last_insert_sequence.keys.empty())
       return;

    IncrementalInserter inserter(context.editor(), last_insert_sequence.mode);
    size_t index = 0;
    insert_sequence(inserter,
                    [&]() { return last_insert_sequence.keys[index++]; },
                    [](){});
}

template<bool append>
void do_go(const Context& context)
{
    int count = context.numeric_param();
    Editor& editor = context.editor();
    if (count != 0)
    {
        BufferIterator target =
            editor.buffer().iterator_at(BufferCoord(count-1, 0));

        editor.select(target);
    }
    else
    {
        Key key = get_key();
        if (key.modifiers != Key::Modifiers::None)
            return;

        switch (key.key)
        {
        case 'g':
        case 't':
        {
            BufferIterator target =
                editor.buffer().iterator_at(BufferCoord(0,0));
            editor.select(target);
            break;
        }
        case 'l':
        case 'L':
            editor.select(select_to_eol, append);
            break;
        case 'h':
        case 'H':
            editor.select(select_to_eol_reverse, append);
            break;
        case 'b':
        {
            BufferIterator target = editor.buffer().iterator_at(
                BufferCoord(editor.buffer().line_count() - 1, 0));
            editor.select(target);
            break;
        }
        }
    }
}

void do_command(const Context& context)
{
    try
    {
        auto cmdline = prompt(":", std::bind(&CommandManager::complete,
                                             &CommandManager::instance(),
                                             _1, _2));

        CommandManager::instance().execute(cmdline, context);
    }
    catch (prompt_aborted&) {}
}

void do_pipe(const Context& context)
{
    try
    {
        auto cmdline = prompt("|", complete_nothing);

        context.buffer().begin_undo_group();
        for (auto& sel : const_cast<const Editor&>(context.editor()).selections())
        {
            String new_content = ShellManager::instance().pipe(String(sel.begin(), sel.end()),
                                                               cmdline, context, {});
            context.buffer().modify(Modification::make_erase(sel.begin(), sel.end()));
            context.buffer().modify(Modification::make_insert(sel.begin(), new_content));
        }
        context.buffer().end_undo_group();
    }
    catch (prompt_aborted&) {}
}

template<bool append>
void do_search(const Context& context)
{
    try
    {
        String ex = prompt("/");
        if (ex.empty())
            ex = RegisterManager::instance()['/'][0];
        else
            RegisterManager::instance()['/'] = ex;

        context.editor().select(std::bind(select_next_match, _1, ex), append);
    }
    catch (prompt_aborted&) {}
}

template<bool append>
void do_search_next(const Context& context)
{
    const String& ex = RegisterManager::instance()['/'][0];
    if (not ex.empty())
        context.editor().select(std::bind(select_next_match, _1, ex), append);
    else
        print_status("no search pattern");
}

void do_yank(const Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
}

void do_erase(const Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    context.editor().erase();
}

void do_change(const Context& context)
{
    RegisterManager::instance()['"'] = context.editor().selections_content();
    do_insert<IncrementalInserter::Mode::Change>(context);
}

enum class PasteMode
{
    Before,
    After,
    Replace
};

template<PasteMode paste_mode>
void do_paste(const Context& context)
{
    Editor& editor = context.editor();
    int count = context.numeric_param();
    Register& reg = RegisterManager::instance()['"'];
    if (count == 0)
    {
        if (paste_mode == PasteMode::Before)
            editor.insert(reg);
        else if (paste_mode == PasteMode::After)
            editor.append(reg);
        else if (paste_mode == PasteMode::Replace)
            editor.replace(reg);
    }
    else
    {
        if (paste_mode == PasteMode::Before)
            editor.insert(reg[count-1]);
        else if (paste_mode == PasteMode::After)
            editor.append(reg[count-1]);
        else if (paste_mode == PasteMode::Replace)
            editor.replace(reg[count-1]);
    }
}

void do_select_regex(const Context& context)
{
    try
    {
        String ex = prompt("select: ");
        context.editor().multi_select(std::bind(select_all_matches, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_split_regex(const Context& context)
{
    try
    {
        String ex = prompt("split: ");
        context.editor().multi_select(std::bind(split_selection, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_join(const Context& context)
{
    Editor& editor = context.editor();
    editor.select(select_whole_lines);
    editor.select(select_to_eol, true);
    editor.multi_select(std::bind(select_all_matches, _1, "\n\\h*"));
    editor.replace(" ");
    editor.clear_selections();
    editor.move_selections({0, -1});
}

template<bool inner>
void do_select_object(const Context& context)
{
    typedef std::function<SelectionAndCaptures (const Selection&)> Selector;
    static const std::unordered_map<Key, Selector> key_to_selector =
    {
        { { Key::Modifiers::None, '(' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '(', ')' }, inner) },
        { { Key::Modifiers::None, ')' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '(', ')' }, inner) },
        { { Key::Modifiers::None, 'b' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '(', ')' }, inner) },
        { { Key::Modifiers::None, '{' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '{', '}' }, inner) },
        { { Key::Modifiers::None, '}' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '{', '}' }, inner) },
        { { Key::Modifiers::None, 'B' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '{', '}' }, inner) },
        { { Key::Modifiers::None, '[' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '[', ']' }, inner) },
        { { Key::Modifiers::None, ']' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '[', ']' }, inner) },
        { { Key::Modifiers::None, '<' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '<', '>' }, inner) },
        { { Key::Modifiers::None, '>' }, std::bind(select_surrounding, _1, std::pair<char, char>{ '<', '>' }, inner) },
        { { Key::Modifiers::None, 'w' }, std::bind(select_whole_word<false>, _1, inner) },
        { { Key::Modifiers::None, 'W' }, std::bind(select_whole_word<true>, _1, inner) },
    };

    Key key = get_key();
    auto it = key_to_selector.find(key);
    if (it != key_to_selector.end())
        context.editor().select(it->second);
}

std::unordered_map<Key, std::function<void (const Context& context)>> keymap =
{
    { { Key::Modifiers::None, 'h' }, [](const Context& context) { context.editor().move_selections(BufferCoord(0, -std::max(context.numeric_param(),1))); } },
    { { Key::Modifiers::None, 'j' }, [](const Context& context) { context.editor().move_selections(BufferCoord( std::max(context.numeric_param(),1), 0)); } },
    { { Key::Modifiers::None, 'k' }, [](const Context& context) { context.editor().move_selections(BufferCoord(-std::max(context.numeric_param(),1), 0)); } },
    { { Key::Modifiers::None, 'l' }, [](const Context& context) { context.editor().move_selections(BufferCoord(0,  std::max(context.numeric_param(),1))); } },

    { { Key::Modifiers::None, 'H' }, [](const Context& context) { context.editor().move_selections(BufferCoord(0, -std::max(context.numeric_param(),1)), true); } },
    { { Key::Modifiers::None, 'J' }, [](const Context& context) { context.editor().move_selections(BufferCoord( std::max(context.numeric_param(),1), 0), true); } },
    { { Key::Modifiers::None, 'K' }, [](const Context& context) { context.editor().move_selections(BufferCoord(-std::max(context.numeric_param(),1), 0), true); } },
    { { Key::Modifiers::None, 'L' }, [](const Context& context) { context.editor().move_selections(BufferCoord(0,  std::max(context.numeric_param(),1)), true); } },

    { { Key::Modifiers::None, 't' }, [](const Context& context) { context.editor().select(std::bind(select_to, _1, get_key().key, context.numeric_param(), false)); } },
    { { Key::Modifiers::None, 'f' }, [](const Context& context) { context.editor().select(std::bind(select_to, _1, get_key().key, context.numeric_param(), true)); } },
    { { Key::Modifiers::None, 'T' }, [](const Context& context) { context.editor().select(std::bind(select_to, _1, get_key().key, context.numeric_param(), false), true); } },
    { { Key::Modifiers::None, 'F' }, [](const Context& context) { context.editor().select(std::bind(select_to, _1, get_key().key, context.numeric_param(), true), true); } },

    { { Key::Modifiers::None, 'd' }, do_erase },
    { { Key::Modifiers::None, 'c' }, do_change },
    { { Key::Modifiers::None, 'i' }, do_insert<IncrementalInserter::Mode::Insert> },
    { { Key::Modifiers::None, 'I' }, do_insert<IncrementalInserter::Mode::InsertAtLineBegin> },
    { { Key::Modifiers::None, 'a' }, do_insert<IncrementalInserter::Mode::Append> },
    { { Key::Modifiers::None, 'A' }, do_insert<IncrementalInserter::Mode::AppendAtLineEnd> },
    { { Key::Modifiers::None, 'o' }, do_insert<IncrementalInserter::Mode::OpenLineBelow> },
    { { Key::Modifiers::None, 'O' }, do_insert<IncrementalInserter::Mode::OpenLineAbove> },

    { { Key::Modifiers::None, 'g' }, do_go<false> },
    { { Key::Modifiers::None, 'G' }, do_go<true> },

    { { Key::Modifiers::None, 'y' }, do_yank },
    { { Key::Modifiers::None, 'p' }, do_paste<PasteMode::After> },
    { { Key::Modifiers::None, 'P' }, do_paste<PasteMode::Before> },
    { { Key::Modifiers::Alt,  'p' }, do_paste<PasteMode::Replace> },

    { { Key::Modifiers::None, 's' }, do_select_regex },


    { { Key::Modifiers::None, '.' }, do_repeat_insert },

    { { Key::Modifiers::None, '%' }, [](const Context& context) { context.editor().clear_selections(); context.editor().select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, do_command },
    { { Key::Modifiers::None, '|' }, do_pipe },
    { { Key::Modifiers::None, ' ' }, [](const Context& context) { int count = context.numeric_param(); if (count == 0) context.editor().clear_selections();
                                                                     else context.editor().keep_selection(count-1); } },
    { { Key::Modifiers::Alt,  ' ' }, [](const Context& context) { int count = context.numeric_param(); if (count == 0) context.editor().clear_selections();
                                                                     else context.editor().remove_selection(count-1); } },
    { { Key::Modifiers::None, 'w' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'e' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word_end<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'b' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_previous_word<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'W' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'E' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word_end<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'B' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_previous_word<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'x' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_line, false); } while(--count > 0); } },
    { { Key::Modifiers::None, 'X' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_line, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'm' }, [](const Context& context) { int count = context.numeric_param(); context.editor().select(select_matching); } },
    { { Key::Modifiers::None, 'M' }, [](const Context& context) { int count = context.numeric_param(); context.editor().select(select_matching, true); } },

    { { Key::Modifiers::None, '/' }, do_search<false> },
    { { Key::Modifiers::None, '?' }, do_search<true> },
    { { Key::Modifiers::None, 'n' }, do_search_next<false> },
    { { Key::Modifiers::None, 'N' }, do_search_next<true> },

    { { Key::Modifiers::None, 'u' }, [](const Context& context) { int count = context.numeric_param(); do { if (not context.editor().undo()) { print_status("nothing left to undo"); break; } } while(--count > 0); } },
    { { Key::Modifiers::None, 'U' }, [](const Context& context) { int count = context.numeric_param(); do { if (not context.editor().redo()) { print_status("nothing left to redo"); break; } } while(--count > 0); } },

    { { Key::Modifiers::Alt,  'i' }, do_select_object<true> },
    { { Key::Modifiers::Alt,  'a' }, do_select_object<false> },

    { { Key::Modifiers::Alt, 't' }, [](const Context& context) { context.editor().select(std::bind(select_to_reverse, _1, get_key().key, context.numeric_param(), false)); } },
    { { Key::Modifiers::Alt, 'f' }, [](const Context& context) { context.editor().select(std::bind(select_to_reverse, _1, get_key().key, context.numeric_param(), true)); } },
    { { Key::Modifiers::Alt, 'T' }, [](const Context& context) { context.editor().select(std::bind(select_to_reverse, _1, get_key().key, context.numeric_param(), false), true); } },
    { { Key::Modifiers::Alt, 'F' }, [](const Context& context) { context.editor().select(std::bind(select_to_reverse, _1, get_key().key, context.numeric_param(), true), true); } },

    { { Key::Modifiers::Alt, 'w' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'e' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word_end<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'b' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_previous_word<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'W' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word<true>, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'E' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_next_word_end<true>, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'B' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_previous_word<true>, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 'l' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_eol, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'L' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_eol, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'h' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_eol_reverse, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'H' }, [](const Context& context) { int count = context.numeric_param(); do { context.editor().select(select_to_eol_reverse, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 's' }, do_split_regex },

    { { Key::Modifiers::Alt, 'j' }, do_join },

    { { Key::Modifiers::Alt, 'x' }, [](const Context& context) { context.editor().select(select_whole_lines); } },
};

}

void run_unit_tests();

int main(int argc, char* argv[])
{
    GlobalOptionManager option_manager;
    GlobalHookManager   hook_manager;
    ShellManager        shell_manager;
    CommandManager      command_manager;
    BufferManager       buffer_manager;
    RegisterManager     register_manager;
    HighlighterRegistry highlighter_registry;
    FilterRegistry      filter_registry;

    run_unit_tests();

    shell_manager.register_env_var("bufname",
                                   [](const String& name, const Context& context)
                                   { return context.buffer().name(); });
    shell_manager.register_env_var("selection",
                                   [](const String& name, const Context& context)
                                   { return context.window().selections_content().back(); });
    shell_manager.register_env_var("opt_.+",
                                   [](const String& name, const Context& context)
                                   { return context.option_manager()[name.substr(4)].as_string(); });

    register_manager.register_dynamic_register('%', [&]() { return std::vector<String>(1, main_context.buffer().name()); });
    register_manager.register_dynamic_register('.', [&]() { return main_context.window().selections_content(); });

    register_commands();
    register_highlighters();
    register_filters();

    try
    {
        NCursesClient client;
        current_client = &client;

        try
        {
            command_manager.execute("runtime kakrc", main_context);
        }
        catch (Kakoune::runtime_error& error)
        {
            print_status(error.description());
        }


        write_debug("*** This is the debug buffer, where debug info will be written ***\n");
        write_debug("utf-8 test: é á ï");

        if (argc > 1)
            command_manager.execute(String("edit ") + argv[1], main_context);
        else
        {
            auto buffer = new Buffer("*scratch*", Buffer::Type::Scratch);
            main_context = Context(*buffer->get_or_create_window());
        }

        current_client->draw_window(main_context.window());
        int count = 0;
        while(not quit_requested)
        {
            try
            {
                Key key = get_key();
                if (key.modifiers == Key::Modifiers::None and isdigit(key.key))
                    count = count * 10 + key.key - '0';
                else
                {
                    auto it = keymap.find(key);
                    if (it != keymap.end())
                    {
                        main_context.numeric_param(count);
                        it->second(main_context);
                        current_client->draw_window(main_context.window());
                    }
                    count = 0;
                }
            }
            catch (Kakoune::runtime_error& error)
            {
                print_status(error.description());
            }
        }
    }
    catch (Kakoune::exception& error)
    {
        puts("uncaught exception:\n");
        puts(error.description().c_str());
        return -1;
    }
    main_context = Context();
    return 0;
}
