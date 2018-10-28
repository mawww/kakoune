#include "parameters_parser.hh"

#include "flags.hh"

namespace Kakoune
{

String generate_switches_doc(const SwitchMap& switches)
{
    String res;
    if (switches.empty())
        return res;

    auto switch_len = [](auto& sw) { return sw.key.column_length() + (sw.value.takes_arg ? 5 : 0); };
    auto switches_len = switches | transform(switch_len);
    const ColumnCount maxlen = *std::max_element(switches_len.begin(), switches_len.end());

    for (auto& sw : switches) {
        res += format("-{} {}{}{}\n",
                      sw.key,
                      sw.value.takes_arg ? "<arg>" : "",
                      String{' ', maxlen - switch_len(sw) + 1},
                      sw.value.description);
    }
    return res;
}

ParametersParser::ParametersParser(ParameterList params, const ParameterDesc& desc)
    : m_params(params),
      m_desc(desc)
{
    const bool switches_only_at_start = desc.flags & ParameterDesc::Flags::SwitchesOnlyAtStart;
    const bool ignore_unknown_switches = desc.flags & ParameterDesc::Flags::IgnoreUnknownSwitches;
    bool only_pos = desc.flags & ParameterDesc::Flags::SwitchesAsPositional;

    Vector<bool> switch_seen(desc.switches.size(), false);
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (not only_pos and not ignore_unknown_switches and params[i] == "--")
            only_pos = true;
        else if (not only_pos and not params[i].empty() and params[i][0_byte] == '-')
        {
            auto it = m_desc.switches.find(params[i].substr(1_byte));
            if (it == m_desc.switches.end())
            {
                if (ignore_unknown_switches)
                {
                    m_positional_indices.push_back(i);
                    if (switches_only_at_start)
                        only_pos = true;
                    continue;
                }
                throw unknown_option(params[i]);
            }

            auto switch_index = it - m_desc.switches.begin();
            if (switch_seen[switch_index])
                throw runtime_error{format("switch '-{}' specified more than once", it->key)};
            switch_seen[switch_index] = true;

            if (it->value.takes_arg and ++i == params.size())
               throw missing_option_value(it->key);
        }
        else // positional
        {
            if (switches_only_at_start)
                only_pos = true;
            m_positional_indices.push_back(i);
        }
    }
    size_t count = m_positional_indices.size();
    if (count > desc.max_positionals or count < desc.min_positionals)
        throw wrong_argument_count();
}

Optional<StringView> ParametersParser::get_switch(StringView name) const
{
    auto it = m_desc.switches.find(name);
    kak_assert(it != m_desc.switches.end());
    for (size_t i = 0; i < m_params.size(); ++i)
    {
        const auto& param = m_params[i];
        if (param.substr(0_byte, 1_byte) == "-" and param.substr(1_byte) == name)
            return it->value.takes_arg ? m_params[i+1] : StringView{};

        if (param == "--")
            break;
    }
    return {};
}

}
