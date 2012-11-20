#include "buffer.hh"
#include "assert.hh"
#include "editor.hh"
#include "selectors.hh"

using namespace Kakoune;

void test_buffer()
{
    Buffer buffer("test", Buffer::Flags::None, "allo ?\nmais que fais la police\n hein ?\n youpi\n");
    assert(buffer.line_count() == 4);

    BufferIterator i = buffer.begin();
    assert(*i == 'a');
    i += 6;
    assert(buffer.line_and_column_at(i) == BufferCoord{0 COMMA 6});
    i += 1;
    assert(buffer.line_and_column_at(i) == BufferCoord{1 COMMA 0});
    --i;
    assert(buffer.line_and_column_at(i) == BufferCoord{0 COMMA 6});
    ++i;
    assert(buffer.line_and_column_at(i) == BufferCoord{1 COMMA 0});
    buffer.insert(i, "tchou kanaky\n");
    assert(buffer.line_count() == 5);

    BufferIterator begin = buffer.iterator_at({ 4, 1 });
    BufferIterator end = buffer.iterator_at({ 4, 5 }) + 1;
    String str = buffer.string(begin, end);
    assert(str == "youpi");

    // check insert at end behaviour: auto add end of line if necessary
    begin = buffer.end() - 1;
    buffer.insert(buffer.end(), "tchou");
    assert(buffer.string(begin+1, buffer.end()) == "tchou\n");

    begin = buffer.end() - 1;
    buffer.insert(buffer.end(), "kanaky\n");
    assert(buffer.string(begin+1, buffer.end()) == "kanaky\n");
}

void test_editor()
{
    Buffer buffer("test", Buffer::Flags::None, "test\n\nyoupi\n");
    Editor editor(buffer);

    using namespace std::placeholders;

    editor.select(select_whole_buffer);
    editor.multi_select(std::bind(select_all_matches, _1, "\\n\\h*"));
    for (auto& sel : editor.selections())
    {
        assert(*sel.begin() == '\n');
        editor.buffer().erase(sel.begin(), sel.end());
    }
}

void test_incremental_inserter()
{
    Buffer buffer("test", Buffer::Flags::None, "test\n\nyoupi\nmatin\n");
    Editor editor(buffer);

    editor.select(buffer.begin());
    {
        IncrementalInserter inserter(editor, InsertMode::OpenLineAbove);
        assert(editor.is_editing());
        assert(editor.selections().size() == 1);
        assert(editor.selections().front().first() == buffer.begin());
        assert(editor.selections().front().last() == buffer.begin());
        assert(*buffer.begin() == L'\n');
    }
    assert(not editor.is_editing());
}

void test_utf8()
{
    String str = "maïs mélange bientôt";
    assert(utf8::distance(str.begin(), str.end()) == 20);
    assert(utf8::codepoint(str.begin() + 2) == 0x00EF);
}

void test_string()
{
   assert(int_to_str(124)  == "124");
   assert(int_to_str(-129) == "-129");
   assert(int_to_str(0)    == "0");

   assert(String("youpi ") + "matin" == "youpi matin");

   std::vector<String> splited = split("youpi:matin::tchou", ':');
   assert(splited[0] == "youpi");
   assert(splited[1] == "matin");
   assert(splited[2] == "");
   assert(splited[3] == "tchou");
}

void run_unit_tests()
{
    test_utf8();
    test_string();
    test_buffer();
    test_editor();
    test_incremental_inserter();
}
