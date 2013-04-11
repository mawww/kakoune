#ifndef idvaluemap_hh_INCLUDED
#define idvaluemap_hh_INCLUDED

#include "completion.hh"

#include <vector>

namespace Kakoune
{

template<typename Id, typename Value>
class idvaluemap
{
public:
    typedef std::pair<Id, Value> value_type;
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

    iterator find(const Id& id)
    {
        for (auto it = begin(); it != end(); ++it)
        {
            if (it->first == id)
                return it;
        }
        return end();
    }

    const_iterator find(const Id& id) const
    {
        for (auto it = begin(); it != end(); ++it)
        {
            if (it->first == id)
                return it;
        }
        return end();
    }

    bool contains(const Id& id) const
    {
        return find(id) != end();
    }

    void remove(const Id& id)
    {
        auto it = find(id);
        if (it != end())
            m_content.erase(it);
    }

    void remove_all(const Id& id)
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

            String id_str = value.first;
            if (id_str.substr(0, real_prefix.length()) == real_prefix)
                result.push_back(std::move(id_str));
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

#endif // idvaluemap_hh_INCLUDED
