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

    BufferIterator pos = buffer.begin();
    kak_assert(*pos == 'a');
    pos += 6;
    kak_assert(pos.coord() == BufferCoord{0 COMMA 6});
    ++pos;
    kak_assert(pos.coord() == BufferCoord{1 COMMA 0});
    --pos;
    kak_assert(pos.coord() == BufferCoord{0 COMMA 6});
    pos += 1;
    kak_assert(pos.coord() == BufferCoord{1 COMMA 0});
    buffer.insert(pos, "tchou kanaky\n");
    kak_assert(buffer.line_count() == 5);

    String str = buffer.string({ 4, 1 }, buffer.next({ 4, 5 }));
    kak_assert(str == "youpi");

    // check insert at end behaviour: auto add end of line if necessary
    pos = buffer.end()-1;
    buffer.insert(pos, "tchou");
    kak_assert(buffer.string(pos.coord(), buffer.end_coord()) == "tchou\n");

    pos = buffer.end()-1;
    buffer.insert(buffer.end(), "kanaky\n");
    kak_assert(buffer.string((pos+1).coord(), buffer.end_coord()) == "kanaky\n");

    buffer.commit_undo_group();
    buffer.erase(pos+1, buffer.end());
    buffer.insert(buffer.end(), "mutch\n");
    buffer.commit_undo_group();
    buffer.undo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -7), buffer.end_coord()) == "kanaky\n");
    buffer.redo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -6), buffer.end_coord()) == "mutch\n");
}

void test_undo_group_optimizer()
{
    std::vector<String> lines = { "allo ?\n", "mais que fais la police\n",  " hein ?\n", " youpi\n" };
    Buffer buffer("test", Buffer::Flags::None, lines);
    auto pos = buffer.insert(buffer.end(), "kanaky\n");
    buffer.erase(pos, buffer.end());
    buffer.insert(buffer.iterator_at(2_line), "tchou\n");
    buffer.insert(buffer.iterator_at(2_line), "mutch\n");
    buffer.erase(buffer.iterator_at({2, 1}), buffer.iterator_at({2, 5}));
    buffer.erase(buffer.iterator_at(2_line), buffer.end());
    buffer.insert(buffer.end(), "youpi");
    buffer.undo();
    buffer.redo();
    buffer.undo();

    kak_assert((int)buffer.line_count() == lines.size());
    for (size_t i = 0; i < lines.size(); ++i)
        kak_assert(lines[i] == buffer[LineCount((int)i)]);
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

    std::vector<String> splited = split("youpi:matin::tchou\\:kanaky:hihi\\:", ':', '\\');
    kak_assert(splited[0] == "youpi");
    kak_assert(splited[1] == "matin");
    kak_assert(splited[2] == "");
    kak_assert(splited[3] == "tchou:kanaky");
    kak_assert(splited[4] == "hihi:");

    String escaped = escape("youpi:matin:tchou:", ':', '\\');
    kak_assert(escaped == "youpi\\:matin\\:tchou\\:");

    kak_assert(prefix_match("tchou kanaky", "tchou"));
    kak_assert(prefix_match("tchou kanaky", "tchou kanaky"));
    kak_assert(prefix_match("tchou kanaky", "t"));
    kak_assert(not prefix_match("tchou kanaky", "c"));

    kak_assert(subsequence_match("tchou kanaky", "tknky"));
    kak_assert(subsequence_match("tchou kanaky", "knk"));
    kak_assert(subsequence_match("tchou kanaky", "tchou kanaky"));
    kak_assert(not subsequence_match("tchou kanaky", "tchou  kanaky"));
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
    test_undo_group_optimizer();
}
