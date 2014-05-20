#include "assert.hh"
#include "buffer.hh"
#include "keys.hh"
#include "selectors.hh"
#include "word_db.hh"

#include "modification.hh"

using namespace Kakoune;

void test_buffer()
{
    Buffer empty_buffer("empty", Buffer::Flags::None, {});

    Buffer buffer("test", Buffer::Flags::None, { "allo ?\n", "mais que fais la police\n",  " hein ?\n", " youpi\n" });
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

void test_word_db()
{
    Buffer buffer("test", Buffer::Flags::None,
                  { "tchou mutch\n",
                    "tchou kanaky tchou\n",
                    "\n",
                    "tchaa tchaa\n",
                    "allo\n"});
    WordDB word_db(buffer);
    auto res = word_db.find_prefix("");
    std::sort(res.begin(), res.end());
    kak_assert(res == std::vector<String>{ "allo" COMMA "kanaky" COMMA "mutch" COMMA "tchaa" COMMA "tchou" });
    kak_assert(word_db.get_word_occurences("tchou") == 3);
    kak_assert(word_db.get_word_occurences("allo") == 1);
    buffer.erase(buffer.iterator_at({1, 6}), buffer.iterator_at({4, 0}));
    res = word_db.find_prefix("");
    std::sort(res.begin(), res.end());
    kak_assert(res == std::vector<String>{ "allo" COMMA "mutch" COMMA "tchou" });
    buffer.insert(buffer.iterator_at({1, 0}), "re");
    res = word_db.find_prefix("");
    std::sort(res.begin(), res.end());
    kak_assert(res == std::vector<String>{ "allo" COMMA "mutch" COMMA "retchou" COMMA "tchou" });
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

void test_modification()
{
    {
        Modification modif = { {5, 10}, {5, 10}, {0, 0}, {4, 17} };
        auto pos = modif.get_new_coord({5, 10});
        kak_assert(pos == ByteCoord{9 COMMA 17});
    }
    {
        Modification modif = { {7, 10}, {7, 10}, {0, 5}, {0, 0} };
        auto pos = modif.get_new_coord({7, 10});
        kak_assert(pos == ByteCoord{7 COMMA 10});
    }
    {
        std::vector<Buffer::Change> change = {
            { Buffer::Change::Insert, {1, 0}, {5, 161}, false },
            { Buffer::Change::Insert, {5, 161}, {30, 0}, false },
            { Buffer::Change::Insert, {30, 0}, {35, 0}, false },
        };
        auto modifs = compute_modifications(change);
        kak_assert(modifs.size() == 1);
        auto& modif = modifs[0];
        kak_assert(modif.old_coord == ByteCoord{1 COMMA 0});
        kak_assert(modif.new_coord == ByteCoord{1 COMMA 0});
        kak_assert(modif.num_added == ByteCoord{34 COMMA 0});
        kak_assert(modif.num_removed == ByteCoord{0 COMMA 0});
    }

    Buffer buffer("test", Buffer::Flags::None,
                  { "tchou mutch\n",
                    "tchou kanaky tchou\n",
                    "\n",
                    "tchaa tchaa\n",
                    "allo\n"});

    size_t timestamp = buffer.timestamp();

    buffer.erase(buffer.iterator_at({0,0}), buffer.iterator_at({3,0}));
    buffer.insert(buffer.iterator_at({0,0}), "youuhou\nniahaha");

    buffer.insert(buffer.iterator_at({2,4}), "yeehaah\n");

    auto modifs = compute_modifications(buffer, timestamp);
    kak_assert(modifs.size() == 2);
    {
        auto& modif = modifs[0];
        kak_assert(modif.old_coord == ByteCoord{0 COMMA 0});
        kak_assert(modif.new_coord == ByteCoord{0 COMMA 0});
        kak_assert(modif.num_added == ByteCoord{1 COMMA 7});
        kak_assert(modif.num_removed == ByteCoord{3 COMMA 0});
        bool deleted;
        auto new_coord = modif.get_new_coord({1, 10}, deleted);
        kak_assert(new_coord == ByteCoord{1 COMMA 7});
        kak_assert(deleted);
    }
    {
        auto& modif = modifs[1];
        kak_assert(modif.old_coord == ByteCoord{4 COMMA 4});
        kak_assert(modif.new_coord == ByteCoord{2 COMMA 4});
        kak_assert(modif.num_added == ByteCoord{1 COMMA 0});
        kak_assert(modif.num_removed == ByteCoord{0 COMMA 0});
    }
}

void run_unit_tests()
{
    test_utf8();
    test_string();
    test_keys();
    test_buffer();
    test_undo_group_optimizer();
    test_modification();
    test_word_db();
}
