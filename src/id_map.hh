#ifndef id_map_hh_INCLUDED
#define id_map_hh_INCLUDED

#include "completion.hh"
#include "string.hh"

#include <vector>

namespace Kakoune
{

template<typename Value>
class id_map
{
public:
    typedef std::pair<String, Value> value_type;
    typedef std::vector<value_type> container_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;

    void append(const value_type& value)
    {
        m_content.push_back(value);
    }

    void append(value_type&& value)
    {
        m_content.push_back(std::move(value));
    }

    iterator find(const String& id)
    {
        for (auto it = begin(); it != end(); ++it)
        {
            if (it->first == id)
                return it;
        }
        return end();
    }

    const_iterator find(const String& id) const
    {
        for (auto it = begin(); it != end(); ++it)
        {
            if (it->first == id)
                return it;
        }
        return end();
    }

    bool contains(const String& id) const
    {
        return find(id) != end();
    }

    void remove(const String& id)
    {
        auto it = find(id);
        if (it != end())
            m_content.erase(it);
    }

    void remove_all(const String& id)
    {
        for (auto it = find(id); it != end(); it = find(id))
            m_content.erase(it);
    }

    template<typename Condition>
    CandidateList complete_id_if(const String& prefix,
                                 ByteCount cursor_pos,
                                 Condition condition) const
    {
        String real_prefix = prefix.substr(0, cursor_pos);
        CandidateList result;
        for (auto& value : m_content)
        {
            if (not condition(value))
                continue;

            if (prefix_match(value.first, real_prefix))
                result.push_back(value.first);
        }
        return result;
    }

    CandidateList complete_id(const String& prefix,
                              ByteCount cursor_pos) const
    {
        return complete_id_if(
            prefix, cursor_pos, [](const value_type&) { return true; });
    }

    iterator       begin()       { return m_content.begin(); }
    iterator       end()         { return m_content.end(); }
    const_iterator begin() const { return m_content.begin(); }
    const_iterator end()   const { return m_content.end(); }

private:
    container_type m_content;
};

}

#endif // id_map_hh_INCLUDED
