#ifndef id_map_hh_INCLUDED
#define id_map_hh_INCLUDED

#include "containers.hh"
#include "string.hh"

#include <vector>

namespace Kakoune
{

template<typename Value>
class IdMap
{
public:
    using value_type = std::pair<String, Value>;
    using container_type = std::vector<value_type>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    IdMap() = default;
    IdMap(std::initializer_list<value_type> val) : m_content{val} {}

    void append(const value_type& value)
    {
        m_content.push_back(value);
    }

    void append(value_type&& value)
    {
        m_content.push_back(std::move(value));
    }

    iterator find(StringView id)
    {
        return Kakoune::find(transformed(m_content, get_id), id).base();
    }

    const_iterator find(StringView id) const
    {
        return Kakoune::find(transformed(m_content, get_id), id).base();
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
        auto it = std::remove_if(begin(), end(),
                                 [&](value_type& v){ return v.first == id; });
        m_content.erase(it, end());
    }

    static const String& get_id(const value_type& v) { return v.first; }

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
