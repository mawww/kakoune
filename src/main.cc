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

bool insert_char(IncrementalInserter& inserter, const Key& key)
{
    switch (key.modifiers)
    {
    case Key::Modifiers::None:
        switch (key.key)
        {
        case 27:
            return false;
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
            last_insert_sequence.keys.push_back(next_key);
            if (next_key.modifiers == Key::Modifiers::None)
            {
                switch (next_key.key)
                {
                case '%':
                    inserter.insert(inserter.buffer().name());
                    break;
                default:
                    inserter.insert(RegisterManager::instance()[next_key.key]);
                }
            }
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
    default:
        break;
    }
    return true;
}

void do_insert(Editor& editor, IncrementalInserter::Mode mode)
{
    last_insert_sequence.mode = mode;
    last_insert_sequence.keys.clear();
    IncrementalInserter inserter(editor, mode);
    draw_editor_ifn(editor);
    while(true)
    {
        Key key = get_key();

        if (not insert_char(inserter, key))
            break;

        last_insert_sequence.keys.push_back(key);
        draw_editor_ifn(editor);
    }
}

void do_repeat_insert(Editor& editor, int count)
{
    IncrementalInserter inserter(editor, last_insert_sequence.mode);
    for (const Key& key : last_insert_sequence.keys)
    {
        insert_char(inserter, key);
    }
    draw_editor_ifn(editor);
}


template<bool append>
void do_go(Editor& editor, int count)
{
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

void do_command()
{
    try
    {
        auto cmdline = prompt(":", std::bind(&CommandManager::complete,
                                             &CommandManager::instance(),
                                             _1, _2));

        CommandManager::instance().execute(cmdline, main_context);
    }
    catch (prompt_aborted&) {}
}

void do_pipe(Editor& editor, int count)
{
    try
    {
        auto cmdline = prompt("|", complete_nothing);

        editor.buffer().begin_undo_group();
        for (auto& sel : const_cast<const Editor&>(editor).selections())
        {
            String new_content = ShellManager::instance().pipe(String(sel.begin(), sel.end()),
                                                               cmdline, main_context, {});
            editor.buffer().modify(Modification::make_erase(sel.begin(), sel.end()));
            editor.buffer().modify(Modification::make_insert(sel.begin(), new_content));
        }
        editor.buffer().end_undo_group();
    }
    catch (prompt_aborted&) {}
}

template<bool append>
void do_search(Editor& editor)
{
    try
    {
        String ex = prompt("/");
        if (ex.empty())
            ex = RegisterManager::instance()['/'].get();
        else
            RegisterManager::instance()['/'] = ex;

        editor.select(std::bind(select_next_match, _1, ex), append);
    }
    catch (prompt_aborted&) {}
}

template<bool append>
void do_search_next(Editor& editor)
{
    const String& ex = RegisterManager::instance()['/'].get();
    if (not ex.empty())
        editor.select(std::bind(select_next_match, _1, ex), append);
    else
        print_status("no search pattern");
}

void do_yank(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
}

void do_erase(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
    editor.erase();
}

void do_change(Editor& editor, int count)
{
    RegisterManager::instance()['"'] = editor.selections_content();
    do_insert(editor, IncrementalInserter::Mode::Change);
}

template<bool append>
void do_paste(Editor& editor, int count)
{
    Register& reg = RegisterManager::instance()['"'];
    if (count == 0)
    {
        if (append)
            editor.append(reg);
        else
            editor.insert(reg);
    }
    else
    {
        if (append)
            editor.append(reg.get(count-1));
        else
            editor.insert(reg.get(count-1));
    }
}

void do_select_regex(Editor& editor, int count)
{
    try
    {
        String ex = prompt("select: ");
        editor.multi_select(std::bind(select_all_matches, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_split_regex(Editor& editor, int count)
{
    try
    {
        String ex = prompt("split: ");
        editor.multi_select(std::bind(split_selection, _1, ex));
    }
    catch (prompt_aborted&) {}
}

void do_join(Editor& editor, int count)
{
    editor.select(select_whole_lines);
    editor.select(select_to_eol, true);
    editor.multi_select(std::bind(select_all_matches, _1, "\n\\h*"));
    editor.replace(" ");
    editor.clear_selections();
    editor.move_selections({0, -1});
}

template<bool inner>
void do_select_object(Editor& editor, int count)
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
        editor.select(it->second);
}

std::unordered_map<Key, std::function<void (Editor& editor, int count)>> keymap =
{
    { { Key::Modifiers::None, 'h' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0, -std::max(count,1))); } },
    { { Key::Modifiers::None, 'j' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord( std::max(count,1), 0)); } },
    { { Key::Modifiers::None, 'k' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(-std::max(count,1), 0)); } },
    { { Key::Modifiers::None, 'l' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0,  std::max(count,1))); } },

    { { Key::Modifiers::None, 'H' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0, -std::max(count,1)), true); } },
    { { Key::Modifiers::None, 'J' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord( std::max(count,1), 0), true); } },
    { { Key::Modifiers::None, 'K' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(-std::max(count,1), 0), true); } },
    { { Key::Modifiers::None, 'L' }, [](Editor& editor, int count) { editor.move_selections(BufferCoord(0,  std::max(count,1)), true); } },

    { { Key::Modifiers::None, 't' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, false)); } },
    { { Key::Modifiers::None, 'f' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, true)); } },
    { { Key::Modifiers::None, 'T' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, false), true); } },
    { { Key::Modifiers::None, 'F' }, [](Editor& editor, int count) { editor.select(std::bind(select_to, _1, get_key().key, count, true), true); } },

    { { Key::Modifiers::None, 'd' }, do_erase },
    { { Key::Modifiers::None, 'c' }, do_change },
    { { Key::Modifiers::None, 'i' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::Insert); } },
    { { Key::Modifiers::None, 'I' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::InsertAtLineBegin); } },
    { { Key::Modifiers::None, 'a' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::Append); } },
    { { Key::Modifiers::None, 'A' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::AppendAtLineEnd); } },
    { { Key::Modifiers::None, 'o' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::OpenLineBelow); } },
    { { Key::Modifiers::None, 'O' }, [](Editor& editor, int count) { do_insert(editor, IncrementalInserter::Mode::OpenLineAbove); } },

    { { Key::Modifiers::None, 'g' }, do_go<false> },
    { { Key::Modifiers::None, 'G' }, do_go<true> },

    { { Key::Modifiers::None, 'y' }, do_yank },
    { { Key::Modifiers::None, 'p' }, do_paste<true> },
    { { Key::Modifiers::None, 'P' }, do_paste<false> },

    { { Key::Modifiers::None, 's' }, do_select_regex },


    { { Key::Modifiers::None, '.' }, do_repeat_insert },

    { { Key::Modifiers::None, '%' }, [](Editor& editor, int count) { editor.clear_selections(); editor.select(select_whole_buffer); } },

    { { Key::Modifiers::None, ':' }, [](Editor& editor, int count) { do_command(); } },
    { { Key::Modifiers::None, '|' }, do_pipe },
    { { Key::Modifiers::None, ' ' }, [](Editor& editor, int count) { if (count == 0) editor.clear_selections();
                                                                     else editor.keep_selection(count-1); } },
    { { Key::Modifiers::Alt,  ' ' }, [](Editor& editor, int count) { if (count == 0) editor.clear_selections();
                                                                     else editor.remove_selection(count-1); } },
    { { Key::Modifiers::None, 'w' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'e' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'b' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<false>); } while(--count > 0); } },
    { { Key::Modifiers::None, 'W' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'E' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'B' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<false>, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'x' }, [](Editor& editor, int count) { do { editor.select(select_line, false); } while(--count > 0); } },
    { { Key::Modifiers::None, 'X' }, [](Editor& editor, int count) { do { editor.select(select_line, true); } while(--count > 0); } },
    { { Key::Modifiers::None, 'm' }, [](Editor& editor, int count) { editor.select(select_matching); } },
    { { Key::Modifiers::None, 'M' }, [](Editor& editor, int count) { editor.select(select_matching, true); } },

    { { Key::Modifiers::None, '/' }, [](Editor& editor, int count) { do_search<false>(editor); } },
    { { Key::Modifiers::None, '?' }, [](Editor& editor, int count) { do_search<true>(editor); } },
    { { Key::Modifiers::None, 'n' }, [](Editor& editor, int count) { do_search_next<false>(editor); } },
    { { Key::Modifiers::None, 'N' }, [](Editor& editor, int count) { do_search_next<true>(editor); } },

    { { Key::Modifiers::None, 'u' }, [](Editor& editor, int count) { do { if (not editor.undo()) { print_status("nothing left to undo"); break; } } while(--count > 0); } },
    { { Key::Modifiers::None, 'U' }, [](Editor& editor, int count) { do { if (not editor.redo()) { print_status("nothing left to redo"); break; } } while(--count > 0); } },

    { { Key::Modifiers::Alt,  'i' }, do_select_object<true> },
    { { Key::Modifiers::Alt,  'a' }, do_select_object<false> },

    { { Key::Modifiers::Alt, 't' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, false)); } },
    { { Key::Modifiers::Alt, 'f' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, true)); } },
    { { Key::Modifiers::Alt, 'T' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, false), true); } },
    { { Key::Modifiers::Alt, 'F' }, [](Editor& editor, int count) { editor.select(std::bind(select_to_reverse, _1, get_key().key, count, true), true); } },

    { { Key::Modifiers::Alt, 'w' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'e' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'b' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<true>); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'W' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word<true>, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'E' }, [](Editor& editor, int count) { do { editor.select(select_to_next_word_end<true>, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'B' }, [](Editor& editor, int count) { do { editor.select(select_to_previous_word<true>, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 'l' }, [](Editor& editor, int count) { do { editor.select(select_to_eol, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'L' }, [](Editor& editor, int count) { do { editor.select(select_to_eol, true); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'h' }, [](Editor& editor, int count) { do { editor.select(select_to_eol_reverse, false); } while(--count > 0); } },
    { { Key::Modifiers::Alt, 'H' }, [](Editor& editor, int count) { do { editor.select(select_to_eol_reverse, true); } while(--count > 0); } },

    { { Key::Modifiers::Alt, 's' }, do_split_regex },

    { { Key::Modifiers::Alt, 'j' }, do_join },

    { { Key::Modifiers::Alt, 'x' }, [](Editor& editor, int count) { editor.select(select_whole_lines); } },
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
    register_commands();
    register_highlighters();
    register_filters();

    try
    {
        NCursesUI ui;
        current_ui = &ui;

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

        current_ui->draw_window(main_context.window());
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
                        it->second(main_context.window(), count);
                        current_ui->draw_window(main_context.window());
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
    return 0;
}
