#ifndef id_map_hh_INCLUDED
#define id_map_hh_INCLUDED

#include "string.hh"
#include "vector.hh"

#include <algorithm>

namespace Kakoune
{

template<typename Value, MemoryDomain domain = MemoryDomain::Undefined>
class IdMap
{
public:
    struct Element
    {
        Element(String k, Value v)
            : hash(hash_value(k)), key(std::move(k)), value(std::move(v)) {}

        size_t hash;
        String key;
        Value value;

        bool operator==(const Element& other) const
        {
            return hash == other.hash and key == other.key and value == other.value;
        }
    };

    using container_type = Vector<Element, domain>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    IdMap() = default;

    IdMap(std::initializer_list<Element> val) : m_content{val} {}

    void append(const Element& value)
    {
        m_content.push_back(value);
    }

    void append(Element&& value)
    {
        m_content.push_back(std::move(value));
    }

    iterator find(StringView id)
    {
        const size_t hash = hash_value(id);
        return std::find_if(begin(), end(),
                            [id, hash](const Element& e)
                            { return e.hash == hash and e.key == id; });
    }

    const_iterator find(StringView id) const
    {
        return const_cast<IdMap*>(this)->find(id);
    }

    bool contains(StringView id) const
    {
        return find(id) != end();
    }

    void remove(StringView id)
    {
        auto it = find(id);
        if (it != end())
            m_content.erase(it);
    }

    void remove_all(StringView id)
    {
        const size_t hash = hash_value(id);
        auto it = std::remove_if(begin(), end(), [id, hash](const Element& e)
                                 { return e.hash == hash and e.key == id; });
        m_content.erase(it, end());
    }

    Value& operator[](StringView id)
    {
        auto it = find(id);
        if (it != m_content.end())
            return it->value;

        append({ id.str(), Value{} });
        return (m_content.end()-1)->value;
    }

    template<MemoryDomain dom>
    bool operator==(const IdMap<Value, dom>& other) const
    {
        return size() == other.size() and std::equal(begin(), end(), other.begin());
    }

    template<MemoryDomain dom>
    bool operator!=(const IdMap<Value, dom>& other) const
    {
        return not (*this == other);
    }

    void reserve(size_t size) { m_content.reserve(size); }
    size_t size() const { return m_content.size(); }
    void clear() { m_content.clear(); }
    void erase(iterator it) { m_content.erase(it); }

    static const String& get_id(const Element& e) { return e.key; }

    bool empty() const { return m_content.empty(); }

    iterator       begin()       { return m_content.begin(); }
    iterator       end()         { return m_content.end(); }
    const_iterator begin() const { return m_content.begin(); }
    const_iterator end()   const { return m_content.end(); }

private:
    container_type m_content;
};

}

#endif // id_map_hh_INCLUDED
