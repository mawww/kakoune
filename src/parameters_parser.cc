#include "parameters_parser.hh"

namespace Kakoune
{

ParametersParser::ParametersParser(const ParameterList& params,
                                   std::unordered_map<String, bool> options,
                                   Flags flags,
                                   size_t min_positionals,
                                   size_t max_positionals)
    : m_params(params),
      m_options(std::move(options))
{
    bool only_pos = false;
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (params[i] == "--")
            only_pos = true;
        else if (not only_pos and params[i][0] == '-')
        {
            auto it = m_options.find(params[i].substr(1_byte));
            if (it == m_options.end())
                throw unknown_option(params[i]);

            if (it->second)
            {
                ++i;
                if (i == params.size() or params[i][0] == '-')
                   throw missing_option_value(params[i]);
            }
        }
        else
        {
            if (flags & Flags::OptionsOnlyAtStart)
                only_pos = true;
            m_positional_indices.push_back(i);
        }
    }
    size_t count = m_positional_indices.size();
    if (count > max_positionals or count < min_positionals)
        throw wrong_argument_count();
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
    return m_positional_indices.size();
}

const String& ParametersParser::operator[] (size_t index) const
{
    assert(index < positional_count());
    return m_params[m_positional_indices[index]];
}

ParametersParser::iterator ParametersParser::begin() const
{
    return iterator(*this, 0);
}

ParametersParser::iterator ParametersParser::end() const
{
    return iterator(*this, m_positional_indices.size());
}

}
