#include "parameters_parser.hh"

namespace Kakoune
{

ParametersParser::ParametersParser(const ParameterList& params,
                                   std::unordered_map<String, bool> options)
    : m_params(params), m_positional(params.size(), true),
      m_options(std::move(options))
{
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (params[i][0] == '-')
        {
            auto it = m_options.find(params[i].substr(1_byte));
            if (it == m_options.end())
                throw unknown_option(params[i]);

            if (it->second)
            {
                if (i + 1 == params.size() or params[i+1][0] == '-')
                   throw missing_option_value(params[i]);

                m_positional[i+1] = false;
            }
            m_positional[i] = false;
        }

        // all options following -- are positional
        if (params[i] == "--")
            break;
    }
}

bool ParametersParser::has_option(const String& name) const
{
    assert(m_options.find(name) != m_options.end());
    for (auto& param : m_params)
    {
        if (param[0] == '-' and param.substr(1_byte) == name)
            return true;

        if (param == "--")
            break;
    }
    return false;
}

const String& ParametersParser::option_value(const String& name) const
{
#ifdef KAK_DEBUG
    auto it = m_options.find(name);
    assert(it != m_options.end());
    assert(it->second == true);
#endif

    for (size_t i = 0; i < m_params.size(); ++i)
    {
        if (m_params[i][0] == '-' and m_params[i].substr(1_byte) == name)
            return m_params[i+1];

        if (m_params[i] == "--")
            break;
    }
    static String empty;
    return empty;
}

size_t ParametersParser::positional_count() const
{
    size_t res = 0;
    for (bool positional : m_positional)
    {
       if (positional)
           ++res;
    }
    return res;
}

const String& ParametersParser::operator[] (size_t index) const
{
    assert(index < positional_count());
    iterator it = begin();
    while (index)
    {
        ++it;
        --index;
    }
    return *it;
}

ParametersParser::iterator ParametersParser::begin() const
{
    int index = 0;
    while (index < m_positional.size() and not m_positional[index])
        ++index;
    return iterator(*this, index);
}

ParametersParser::iterator ParametersParser::end() const
{
    return iterator(*this, m_params.size());
}

}
