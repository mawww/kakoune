#include "buffer.hh"
#include "assert.hh"
#include "editor.hh"
#include "selectors.hh"

using namespace Kakoune;

void test_buffer()
{
    Buffer buffer("test", Buffer::Type::Scratch, "allo ?\nmais que fais la police\n hein ?\n youpi\n");
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
}

void test_editor()
{
    Buffer buffer("test", Buffer::Type::Scratch, "test\n\nyoupi\n");
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
    test_string();
    test_buffer();
    test_editor();
}
