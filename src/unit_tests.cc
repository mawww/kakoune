#include "assert.hh"
#include "buffer.hh"
#include "keys.hh"
#include "selectors.hh"
#include "word_db.hh"
#include "line_modification.hh"

#include <tuple>

using namespace Kakoune;

void test_buffer()
{
    Buffer empty_buffer("empty", Buffer::Flags::None, {});

    Buffer buffer("test", Buffer::Flags::None, { "allo ?\n"_ss, "mais que fais la police\n"_ss,  " hein ?\n"_ss, " youpi\n"_ss });
    kak_assert(buffer.line_count() == 4);

    BufferIterator pos = buffer.begin();
    kak_assert(*pos == 'a');
    pos += 6;
    kak_assert(pos.coord() == ByteCoord{0 COMMA 6});
    ++pos;
    kak_assert(pos.coord() == ByteCoord{1 COMMA 0});
    --pos;
    kak_assert(pos.coord() == ByteCoord{0 COMMA 6});
    pos += 1;
    kak_assert(pos.coord() == ByteCoord{1 COMMA 0});
    buffer.insert(pos, "tchou kanaky\n");
    kak_assert(buffer.line_count() == 5);
    BufferIterator pos2 = buffer.end();
    pos2 -= 9;
    kak_assert(*pos2 == '?');

    String str = buffer.string({ 4, 1 }, buffer.next({ 4, 5 }));
    kak_assert(str == "youpi");

    // check insert at end behaviour: auto add end of line if necessary
    pos = buffer.end()-1;
    buffer.insert(pos, "tchou");
    kak_assert(buffer.string(pos.coord(), buffer.end_coord()) == StringView{"tchou\n"});

    pos = buffer.end()-1;
    buffer.insert(buffer.end(), "kanaky\n");
    kak_assert(buffer.string((pos+1).coord(), buffer.end_coord()) == StringView{"kanaky\n"});

    buffer.commit_undo_group();
    buffer.erase(pos+1, buffer.end());
    buffer.insert(buffer.end(), "mutch\n");
    buffer.commit_undo_group();
    buffer.undo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -7), buffer.end_coord()) == StringView{"kanaky\n"});
    buffer.redo();
    kak_assert(buffer.string(buffer.advance(buffer.end_coord(), -6), buffer.end_coord()) == StringView{"mutch\n"});
}

void test_undo_group_optimizer()
{
    BufferLines lines = { "allo ?\n"_ss, "mais que fais la police\n"_ss,  " hein ?\n"_ss, " youpi\n"_ss };
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
        kak_assert(SharedString{lines[i]} == buffer[LineCount((int)i)]);
}

void test_word_db()
{
    Buffer buffer("test", Buffer::Flags::None,
                  { "tchou mutch\n"_ss,
                    "tchou kanaky tchou\n"_ss,
                    "\n"_ss,
                    "tchaa tchaa\n"_ss,
                    "allo\n"_ss});
    WordDB word_db(buffer);
    auto res = word_db.find_matching("", prefix_match);
    std::sort(res.begin(), res.end());
    kak_assert(res == WordDB::WordList{ "allo" COMMA "kanaky" COMMA "mutch" COMMA "tchaa" COMMA "tchou" });
    kak_assert(word_db.get_word_occurences("tchou") == 3);
    kak_assert(word_db.get_word_occurences("allo") == 1);
    buffer.erase(buffer.iterator_at({1, 6}), buffer.iterator_at({4, 0}));
    res = word_db.find_matching("", prefix_match);
    std::sort(res.begin(), res.end());
    kak_assert(res == WordDB::WordList{ "allo" COMMA "mutch" COMMA "tchou" });
    buffer.insert(buffer.iterator_at({1, 0}), "re");
    res = word_db.find_matching("", subsequence_match);
    std::sort(res.begin(), res.end());
    kak_assert(res == WordDB::WordList{ "allo" COMMA "mutch" COMMA "retchou" COMMA "tchou" });
}

void test_utf8()
{
    String str = "maïs mélange bientôt";
    kak_assert(utf8::distance(str.begin(), str.end()) == 20);
    kak_assert(utf8::codepoint(str.begin() + 2, str.end()) == 0x00EF);
}

void test_string()
{
    kak_assert(String("youpi ") + "matin" == "youpi matin");

    Vector<String> splited = split("youpi:matin::tchou\\:kanaky:hihi\\:", ':', '\\');
    kak_assert(splited[0] == "youpi");
    kak_assert(splited[1] == "matin");
    kak_assert(splited[2] == "");
    kak_assert(splited[3] == "tchou:kanaky");
    kak_assert(splited[4] == "hihi:");

    Vector<StringView> splitedview = split("youpi:matin::tchou\\:kanaky:hihi\\:", ':');
    kak_assert(splitedview[0] == "youpi");
    kak_assert(splitedview[1] == "matin");
    kak_assert(splitedview[2] == "");
    kak_assert(splitedview[3] == "tchou\\");
    kak_assert(splitedview[4] == "kanaky");
    kak_assert(splitedview[5] == "hihi\\");
    kak_assert(splitedview[6] == "");

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

    kak_assert(format("Youhou {1} {} {0} \\{}", 10, "hehe", 5) == "Youhou hehe 5 10 {}");

    char buffer[20];
    kak_assert(format_to(buffer, "Hey {}", 15) == "Hey 15");

    kak_assert(str_to_int("5") == 5);
    kak_assert(str_to_int(to_string(INT_MAX)) == INT_MAX);
    kak_assert(str_to_int(to_string(INT_MIN)) == INT_MIN);
    kak_assert(str_to_int("00") == 0);
    kak_assert(str_to_int("-0") == 0);
}

void test_keys()
{
    KeyList keys{
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

bool operator==(const LineModification& lhs, const LineModification& rhs)
{
    return std::tie(lhs.old_line, lhs.new_line, lhs.num_removed, lhs.num_added) ==
           std::tie(rhs.old_line, rhs.new_line, rhs.num_removed, rhs.num_added);
}

void test_line_modifications()
{
    {
        Buffer buffer("test", Buffer::Flags::None, { "line 1\n"_ss, "line 2\n"_ss });
        auto ts = buffer.timestamp();
        buffer.erase(buffer.iterator_at({1, 0}), buffer.iterator_at({2, 0}));

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 1 COMMA 1 COMMA 1 COMMA 0 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None, { "line 1\n"_ss, "line 2\n"_ss });
        auto ts = buffer.timestamp();
        buffer.insert(buffer.iterator_at({1, 7}), "line 3");

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 2 COMMA 2 COMMA 0 COMMA 1 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None,
                      { "line 1\n"_ss, "line 2\n"_ss, "line 3\n"_ss });

        auto ts = buffer.timestamp();
        buffer.insert(buffer.iterator_at({1, 4}), "hoho\nhehe");
        buffer.erase(buffer.iterator_at({0, 0}), buffer.iterator_at({1, 0}));

        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 2 COMMA 2 });
    }

    {
        Buffer buffer("test", Buffer::Flags::None,
                      { "line 1\n"_ss, "line 2\n"_ss, "line 3\n"_ss, "line 4\n"_ss });

        WordDB word_db(buffer);
        word_db.find_matching("", prefix_match);

        auto ts = buffer.timestamp();
        buffer.erase(buffer.iterator_at({0,0}), buffer.iterator_at({3,0}));
        buffer.insert(buffer.iterator_at({1,0}), "newline 1\nnewline 2\nnewline 3\n");
        buffer.erase(buffer.iterator_at({0,0}), buffer.iterator_at({1,0}));
        {
            auto modifs = compute_line_modifications(buffer, ts);
            kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 4 COMMA 3 });
        }
        buffer.insert(buffer.iterator_at({3,0}), "newline 4\n");

        {
            auto modifs = compute_line_modifications(buffer, ts);
            kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 4 COMMA 4 });
        }

        word_db.find_matching("", prefix_match);
    }

    {
        Buffer buffer("test", Buffer::Flags::None, { "line 1\n"_ss });
        auto ts = buffer.timestamp();
        buffer.insert(buffer.iterator_at({0,0}), "n");
        buffer.insert(buffer.iterator_at({0,1}), "e");
        buffer.insert(buffer.iterator_at({0,2}), "w");
        auto modifs = compute_line_modifications(buffer, ts);
        kak_assert(modifs.size() == 1 && modifs[0] == LineModification{ 0 COMMA 0 COMMA 1 COMMA 1 });
    }
}

void run_unit_tests()
{
    test_utf8();
    test_string();
    test_keys();
    test_buffer();
    test_undo_group_optimizer();
    test_word_db();
    test_line_modifications();
}
