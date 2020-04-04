#ifndef json_hh_INCLUDED
#define json_hh_INCLUDED

#include "hash_map.hh"
#include "string.hh"
#include "value.hh"

namespace Kakoune
{

using JsonArray = Vector<Value>;
using JsonObject = HashMap<String, Value>;

template<typename T>
String to_json(ArrayView<const T> array)
{
    return "[" + join(array | transform([](auto&& elem) { return to_json(elem); }), ", ") + "]";
}

template<typename T, MemoryDomain D>
String to_json(const Vector<T, D>& vec) { return to_json(ArrayView<const T>{vec}); }

template<typename K, typename V, MemoryDomain D>
String to_json(const HashMap<K, V, D>& map)
{
    return "{" + join(map | transform([](auto&& i) { return format("{}: {}", to_json(i.key), to_json(i.value)); }),
                      ',', false) + "}";
}

String to_json(int i);
String to_json(bool b);
String to_json(StringView str);

struct JsonResult { Value value; const char* new_pos; };

JsonResult parse_json(const char* pos, const char* end);
JsonResult parse_json(StringView json);

}

#endif // json_hh_INCLUDED
