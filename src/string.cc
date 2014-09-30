#include "string.hh"

#include "exception.hh"
#include "utils.hh"
#include "utf8_iterator.hh"

namespace Kakoune
{

bool operator<(StringView lhs, StringView rhs)
{
    int cmp = strncmp(lhs.data(), rhs.data(), (int)std::min(lhs.length(), rhs.length()));
    if (cmp == 0)
        return lhs.length() < rhs.length();
    return cmp < 0;
}

std::vector<String> split(StringView str, char separator, char escape)
{
    std::vector<String> res;
    auto it = str.begin();
    while (it != str.end())
    {
        res.emplace_back();
        String& element = res.back();
        while (it != str.end())
        {
            auto c = *it;
            if (c == escape and it + 1 != str.end() and *(it+1) == separator)
            {
                element += separator;
                it += 2;
            }
            else if (c == separator)
            {
                ++it;
                break;
            }
            else
            {
                element += c;
                ++it;
            }
        }
    }
    return res;
}

String escape(StringView str, char character, char escape)
{
    String res;
    for (auto& c : str)
    {
        if (c == character)
            res += escape;
        res += c;
    }
    return res;
}

String escape(StringView str, StringView characters, char escape)
{
    String res;
    for (auto& c : str)
    {
        if (contains(characters, c))
            res += escape;
        res += c;
    }
    return res;
}

int str_to_int(StringView str)
{
    int res = 0;
    if (sscanf(str.zstr(), "%i", &res) != 1)
        throw runtime_error(str + "is not a number");
    return res;
}

String to_string(int val)
{
    char buf[16];
    sprintf(buf, "%i", val);
    return buf;
}

String option_to_string(const Regex& re)
{
    return String{re.str()};
}

void option_from_string(StringView str, Regex& re)
{
    try
    {
        re = Regex{str.begin(), str.end()};
    }
    catch (boost::regex_error& err)
    {
        throw runtime_error("unable to create regex: "_str + err.what());
    }
}

bool prefix_match(StringView str, StringView prefix)
{
    auto it = str.begin();
    for (auto& c : prefix)
    {
        if (it ==str.end() or *it++ != c)
            return false;
    }
    return true;
}

bool subsequence_match(StringView str, StringView subseq)
{
    auto it = str.begin();
    for (auto& c : subseq)
    {
        if (it == str.end())
            return false;
        while (*it != c)
        {
            if (++it == str.end())
                return false;
        }
        ++it;
    }
    return true;
}

String expand_tabs(StringView line, CharCount tabstop, CharCount col)
{
    String res;
    using Utf8It = utf8::iterator<const char*>;
    for (Utf8It it = line.begin(); it.base() < line.end(); ++it)
    {
        if (*it == '\t')
        {
            CharCount end_col = (col / tabstop + 1) * tabstop;
            res += String{' ', end_col - col};
        }
        else
            res += *it;
    }
    return res;
}

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
