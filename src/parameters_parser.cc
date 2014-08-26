#include "parameters_parser.hh"

namespace Kakoune
{

String generate_switches_doc(const SwitchMap& switches)
{
    String res;
    for (auto& sw : switches)
        res += " -" + sw.first + (sw.second.takes_arg ? " <arg>: " : ": ") + sw.second.description + "\n";
    return res;
}

ParametersParser::ParametersParser(ParameterList params,
                                   const ParameterDesc& desc)
    : m_params(params),
      m_desc(desc)
{
    bool only_pos = desc.flags & ParameterDesc::Flags::SwitchesAsPositional;
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (not only_pos and params[i] == "--")
            only_pos = true;
        else if (not only_pos and params[i][0] == '-')
        {
            auto it = m_desc.switches.find(params[i].substr(1_byte));
            if (it == m_desc.switches.end())
                throw unknown_option(params[i]);

            if (it->second.takes_arg)
            {
                ++i;
                if (i == params.size() or params[i][0] == '-')
                   throw missing_option_value(it->first);
            }
        }
        else
        {
            if (desc.flags & ParameterDesc::Flags::SwitchesOnlyAtStart)
                only_pos = true;
            m_positional_indices.push_back(i);
        }
    }
    size_t count = m_positional_indices.size();
    if (count > desc.max_positionals or count < desc.min_positionals)
        throw wrong_argument_count();
}

bool ParametersParser::has_option(const String& name) const
{
    kak_assert(m_desc.switches.find(name) != m_desc.switches.end());
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
    auto it = m_desc.switches.find(name);
    kak_assert(it != m_desc.switches.end());
    kak_assert(it->second.takes_arg);
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
    kak_assert(index < positional_count());
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
