#include "hash.hh"

#include <cstdint>

namespace Kakoune
{

[[gnu::always_inline]]
static inline uint32_t rotl(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

[[gnu::always_inline]]
static inline uint32_t fmix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

// murmur3 hash, based on https://github.com/PeterScott/murmur3
size_t hash_data(const char* input, size_t len)
{
    const uint8_t* data = reinterpret_cast<const uint8_t*>(input);
    uint32_t hash = 0x1235678;
    constexpr uint32_t c1 = 0xcc9e2d51;
    constexpr uint32_t c2 = 0x1b873593;

    const int nblocks = len / 4;
    const uint32_t* blocks = reinterpret_cast<const uint32_t*>(data + nblocks*4);

    for (int i = -nblocks; i; ++i)
    {
        uint32_t key = blocks[i];
        key *= c1;
        key = rotl(key, 15);
        key *= c2;

        hash ^= key;
        hash = rotl(hash, 13);
        hash = hash * 5 + 0xe6546b64;
    }

    const uint8_t* tail = data + nblocks * 4;
    uint32_t key = 0;
    switch (len & 3)
    {
        case 3: key ^= tail[2] << 16;
        case 2: key ^= tail[1] << 8;
        case 1: key ^= tail[0];
                key *= c1;
                key = rotl(key,15);
                key *= c2;
                hash ^= key;
    }

    hash ^= len;
    hash = fmix(hash);

    return hash;
}

}
