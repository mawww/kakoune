#include "hash_map.hh"

#include "clock.hh"
#include "string.hh"
#include "buffer_utils.hh"
#include "unit_tests.hh"

#include <random>
#include <unordered_map>

namespace Kakoune
{

UnitTest test_hash_map{[] {
    // Basic usage
    {
        HashMap<int, int> map;
        map.insert({10, 1});
        map.insert({20, 2});
        kak_assert(map.find_index(0) == -1);
        kak_assert(map.find_index(10) == 0);
        kak_assert(map.find_index(20) == 1);
        kak_assert(map[10] == 1);
        kak_assert(map[20] == 2);
        kak_assert(map[30] == 0);
        map[30] = 3;
        kak_assert(map.find_index(30) == 2);
        map.remove(20);
        kak_assert(map.find_index(30) == 1);
        kak_assert(map.size() == 2);
    }

    // Multiple entries with the same key
    {
        HashMap<int, int> map;
        map.insert({10, 1});
        map.insert({10, 2});
        kak_assert(map.find_index(10) == 0);
        map.remove(10);
        kak_assert(map.find_index(10) == 0);
        map.remove(10);
        kak_assert(map.find_index(10) == -1);
        map.insert({20, 1});
        map.insert({20, 2});
        map.remove_all(20);
        kak_assert(map.find_index(20) == -1);
    }

    // Check hash compatible support
    {
        HashMap<String, int> map;
        map.insert({"test", 10});
        kak_assert(map[StringView{"test"}] == 10);
        map.remove(StringView{"test"});
    }

    // make sure we get what we expect from the hash map
    {
        std::random_device dev;
        std::default_random_engine re{dev()};
        std::uniform_int_distribution<int> dist;

        HashMap<int, int> map;
        Vector<std::pair<int, int>> ref;

        for (int i = 0; i < 100; ++i)
        {
            auto key = dist(re), value = dist(re);
            ref.push_back({key, value});
            map.insert({key, value});

            std::random_shuffle(ref.begin(), ref.end());
            for (auto& elem : ref)
            {
                auto it = map.find(elem.first);
                kak_assert(it != map.end() and it->value == elem.second);
            }
        }
    }
}};

template<typename Map>
void do_profile(size_t count, StringView type)
{
    std::random_device dev;
    std::default_random_engine re{dev()};
    std::uniform_int_distribution<size_t> dist{0, count};

    Vector<size_t> vec;
    for (size_t i = 0; i < count; ++i)
        vec.push_back(i);
    std::random_shuffle(vec.begin(), vec.end());

    Map map;
    auto start = Clock::now();

    for (auto v : vec)
        map.insert({v, dist(re)});
    auto after_insert = Clock::now();

    for (size_t i = 0; i < count; ++i)
        ++map[dist(re)];
    auto after_read = Clock::now();

    for (size_t i = 0; i < count; ++i)
        map.erase(dist(re));
    auto after_remove = Clock::now();

    int c = 0;
    for (auto v : vec)
    {
        auto it = map.find(v);
        if (it != map.end())
            ++c;
    }
    auto after_find = Clock::now();

    write_to_debug_buffer(format("{} ({}) -- inserts: {}ms, reads: {}ms, remove: {}ms, find: {}ms ({})", type, count,
                                 std::chrono::duration_cast<std::chrono::milliseconds>(after_insert - start).count(),
                                 std::chrono::duration_cast<std::chrono::milliseconds>(after_read - after_insert).count(),
                                 std::chrono::duration_cast<std::chrono::milliseconds>(after_remove - after_read).count(),
                                 std::chrono::duration_cast<std::chrono::milliseconds>(after_find - after_remove).count(),
                                 c));
}

void profile_hash_maps()
{
    for (auto i : { 1000, 10000, 100000, 1000000, 10000000 })
    {
        do_profile<std::unordered_map<size_t, size_t>>(i, "UnorderedMap");
        do_profile<HashMap<size_t, size_t>>(i, "     HashMap");
    }
}

}
