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

    BufferCoord pos = {0,0};
    kak_assert(buffer.byte_at(pos) == 'a');
    pos = buffer.advance(pos, 6);
    kak_assert(pos == BufferCoord{0 COMMA 6});
    pos = buffer.next(pos);
    kak_assert(pos == BufferCoord{1 COMMA 0});
    pos = buffer.prev(pos);
    kak_assert(pos == BufferCoord{0 COMMA 6});
    pos = buffer.advance(pos, 1);
    kak_assert(pos == BufferCoord{1 COMMA 0});
    buffer.insert(pos, "tchou kanaky\n");
    kak_assert(buffer.line_count() == 5);

    String str = buffer.string({ 4, 1 }, buffer.next({ 4, 5 }));
    kak_assert(str == "youpi");

    // check insert at end behaviour: auto add end of line if necessary
    pos = buffer.back_coord();
    buffer.insert(pos, "tchou");
    kak_assert(buffer.string(pos, buffer.end_coord()) == "tchou\n");

    pos = buffer.back_coord();
    buffer.insert(buffer.end_coord(), "kanaky\n");
    kak_assert(buffer.string(buffer.next(pos), buffer.end_coord()) == "kanaky\n");

    buffer.commit_undo_group();
    buffer.erase(buffer.next(pos), buffer.end_coord());
    buffer.insert(buffer.end_coord(), "mutch\n");
    buffer.commit_undo_group();
    buffer.undo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -7), buffer.end_coord()) == "kanaky\n");
    buffer.redo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -6), buffer.end_coord()) == "mutch\n");
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
            kak_assert(buffer.byte_at(sel.min()) == '\n');
            erase(buffer, sel);
        }
    }
    editor.undo();

    Selection sel{ 2_line, buffer.back_coord() };
    editor.select(sel, SelectMode::Replace);
    editor.insert("",InsertMode::Replace);
    kak_assert(not buffer.is_end(editor.main_selection().first()));
}

void test_incremental_inserter()
{
    Buffer buffer("test", Buffer::Flags::None, { "test\n", "\n", "youpi\n", "matin\n" });
    Editor editor(buffer);

    editor.select({0,0});
    {
        IncrementalInserter inserter(editor, InsertMode::OpenLineAbove);
        kak_assert(editor.is_editing());
        kak_assert(editor.selections().size() == 1);
        kak_assert(editor.selections().front().first() == BufferCoord{0 COMMA 0});
        kak_assert(editor.selections().front().last() == BufferCoord{0 COMMA 0});
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
