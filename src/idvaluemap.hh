#ifndef idvaluemap_hh_INCLUDED
#define idvaluemap_hh_INCLUDED

namespace Kakoune
{

template<typename _Id, typename _Value>
class idvaluemap
{
public:
    typedef std::pair<_Id, _Value> value_type;
    typedef std::vector<value_type> container_type;
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;

    void append(const value_type& value)
    {
        m_content.push_back(value);
    }

    void append(value_type&& value)
    {
        m_content.push_back(value);
    }

    iterator find(const _Id& id)
    {
        for (auto it = begin(); it != end(); ++it)
        {
            if (it->first == id)
                return it;
        }
        return end();
    }

    const_iterator find(const _Id& id) const
    {
        for (auto it = begin(); it != end(); ++it)
        {
            if (it->first == id)
                return it;
        }
        return end();
    }

    bool contains(const _Id& id) const
    {
        return find(id) != end();
    }

    void remove(const _Id& id)
    {
        for (auto it = m_content.begin(); it != m_content.end(); ++it)
        {
            if (it->first == id)
            {
                m_content.erase(it);
                return;
            }
        }
    }

    template<std::string (*id_to_string)(const _Id&)>
    CandidateList complete_id(const std::string& prefix,
                              size_t cursor_pos)
    {
        std::string real_prefix = prefix.substr(0, cursor_pos);
        CandidateList result;
        for (auto& value : m_content)
        {
            std::string id_str = id_to_string(value.first);
            if (id_str.substr(0, real_prefix.length()) == real_prefix)
                result.push_back(std::move(id_str));
        }
        return result;
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
