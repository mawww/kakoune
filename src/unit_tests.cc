#include "assert.hh"
#include "buffer.hh"
#include "editor.hh"
#include "keys.hh"
#include "selectors.hh"

using namespace Kakoune;

void test_buffer()
{
    Buffer empty_buffer("empty", Buffer::Flags::None, {});

    Buffer buffer("test", Buffer::Flags::None, { "allo ?\n", "mais que fais la police\n",  " hein ?\n", " youpi\n" });
    kak_assert(buffer.line_count() == 4);

    BufferIterator i = buffer.begin();
    kak_assert(*i == 'a');
    i += 6;
    kak_assert(i.coord() == BufferCoord{0 COMMA 6});
    i += 1;
    kak_assert(i.coord() == BufferCoord{1 COMMA 0});
    --i;
    kak_assert(i.coord() == BufferCoord{0 COMMA 6});
    ++i;
    kak_assert(i.coord() == BufferCoord{1 COMMA 0});
    buffer.insert(i, "tchou kanaky\n");
    kak_assert(buffer.line_count() == 5);

    BufferIterator begin = buffer.iterator_at({ 4, 1 });
    BufferIterator end = buffer.iterator_at({ 4, 5 }) + 1;
    String str = buffer.string(begin, end);
    kak_assert(str == "youpi");

    // check insert at end behaviour: auto add end of line if necessary
    begin = buffer.end() - 1;
    buffer.insert(buffer.end(), "tchou");
    kak_assert(buffer.string(begin+1, buffer.end()) == "tchou\n");

    begin = buffer.end() - 1;
    buffer.insert(buffer.end(), "kanaky\n");
    kak_assert(buffer.string(begin+1, buffer.end()) == "kanaky\n");

    buffer.commit_undo_group();
    buffer.erase(begin+1, buffer.end());
    buffer.insert(buffer.end(), "mutch\n");
    buffer.commit_undo_group();
    buffer.undo();
    kak_assert(buffer.string(buffer.end() - 7, buffer.end()) == "kanaky\n");
    buffer.redo();
    kak_assert(buffer.string(buffer.end() - 6, buffer.end()) == "mutch\n");
}

void test_editor()
{
    using namespace std::placeholders;
    Buffer buffer("test", Buffer::Flags::None, { "test\n", "\n", "youpi\n" });
    Editor editor(buffer);

    {
        scoped_edition edition{editor};
        editor.select(select_whole_buffer);
        editor.multi_select(std::bind(select_all_matches, _1, _2, Regex{"\\n\\h*"}));
        for (auto& sel : editor.selections())
        {
            kak_assert(*sel.min() == '\n');
            editor.buffer().erase(sel.min(), utf8::next(sel.max()));
        }
    }
    editor.undo();

    Selection sel{ buffer.iterator_at_line_begin(2_line), buffer.end()-1 };
    editor.select(sel, SelectMode::Replace);
    editor.insert("",InsertMode::Replace);
    kak_assert(not editor.main_selection().first().is_end());
}

void test_incremental_inserter()
{
    Buffer buffer("test", Buffer::Flags::None, { "test\n", "\n", "youpi\n", "matin\n" });
    Editor editor(buffer);

    editor.select(buffer.begin());
    {
        IncrementalInserter inserter(editor, InsertMode::OpenLineAbove);
        kak_assert(editor.is_editing());
        kak_assert(editor.selections().size() == 1);
        kak_assert(editor.selections().front().first() == buffer.begin());
        kak_assert(editor.selections().front().last() == buffer.begin());
        kak_assert(*buffer.begin() == L'\n');
    }
    kak_assert(not editor.is_editing());
}

void test_utf8()
{
    String str = "maïs mélange bientôt";
    kak_assert(utf8::distance(str.begin(), str.end()) == 20);
    kak_assert(utf8::codepoint(str.begin() + 2) == 0x00EF);
}

void test_string()
{
   kak_assert(String("youpi ") + "matin" == "youpi matin");

   std::vector<String> splited = split("youpi:matin::tchou", ':');
   kak_assert(splited[0] == "youpi");
   kak_assert(splited[1] == "matin");
   kak_assert(splited[2] == "");
   kak_assert(splited[3] == "tchou");
}

void test_keys()
{
    std::vector<Key> keys{
         { ' ' },
         { 'c' },
         { Key::Modifiers::Alt, 'j' },
         { Key::Modifiers::Control, 'r' }
    };
    String keys_as_str;
    for (auto& key : keys)
        keys_as_str += key_to_str(key);
    auto parsed_keys = parse_keys(keys_as_str);
    kak_assert(keys == parsed_keys);
}

void run_unit_tests()
{
    test_utf8();
    test_string();
    test_keys();
    test_buffer();
    test_editor();
    test_incremental_inserter();
}
