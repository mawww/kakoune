#include "keys.hh"

#include <unordered_map>

namespace Kakoune
{

static std::unordered_map<std::string, Key> keynamemap = {
    { "ret", { Key::Modifiers::None, '\n' } }
};

KeyList parse_keys(const std::string& str)
{
    KeyList result;
    for (size_t pos = 0; pos < str.length(); ++pos)
    {
        if (str[pos] == '<')
        {
            size_t end_pos = pos;
            while (end_pos < str.length() and str[end_pos] != '>')
                ++end_pos;

            if (end_pos < str.length())
            {
                std::string keyname = str.substr(pos+1, end_pos - pos - 1);
                auto it = keynamemap.find(keyname);
                if (it != keynamemap.end())
                {
                    result.push_back(it->second);
                    pos = end_pos;
                    continue;
                }
            }
        }
        result.push_back({Key::Modifiers::None, str[pos]});
    }
    return result;
}

}
