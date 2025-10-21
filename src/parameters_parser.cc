#include "parameters_parser.hh"

#include "ranges.hh"
#include "flags.hh"

namespace Kakoune
{

String generate_switches_doc(const SwitchMap& switches)
{
    String res;
    if (switches.empty())
        return res;

    auto switch_len = [](auto& sw) { return sw.key.column_length() + (sw.value.arg_completer ? 5 : 0); };
    auto switches_len = switches | transform(switch_len);
    const ColumnCount maxlen = *std::max_element(switches_len.begin(), switches_len.end());

    for (auto& sw : switches) {
        res += format("-{} {}{}{}\n",
                      sw.key,
                      sw.value.arg_completer ? "<arg>" : "",
                      String{' ', maxlen - switch_len(sw) + 1},
                      sw.value.description);
    }
    return res;
}

ParametersParser::ParametersParser(ParameterList params, const ParameterDesc& desc, bool ignore_errors)
    : m_params(params)
{
    const bool switches_only_at_start = desc.flags & ParameterDesc::Flags::SwitchesOnlyAtStart;
    const bool ignore_unknown_switches = desc.flags & ParameterDesc::Flags::IgnoreUnknownSwitches;
    bool only_pos = desc.flags & ParameterDesc::Flags::SwitchesAsPositional;
    bool with_coord = desc.flags & ParameterDesc::Flags::WithCoord;

    Vector<bool> switch_seen(desc.switches.size(), false);
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (not only_pos and not ignore_unknown_switches and params[i] == "--")
        {
            m_state = State::Switch;
            only_pos = true;
        }
        else if (not only_pos and with_coord and not params[i].empty() and params[i][0_byte] == '+')
        {
            m_state = State::Switch;
            with_coord = false;
            const auto coord_str = params[i].substr(1_byte);
            const auto colon = find(coord_str, ':');

            const auto line_str = StringView{coord_str.begin(), colon};
            const LineCount line = line_str.empty() ? INT_MAX : std::max(1, str_to_int(line_str)) - 1;

            ByteCount column = 0;
            if (colon != coord_str.end())
            {
                const auto column_str = StringView{colon + 1, coord_str.end()};
                column = column_str.empty() ? INT_MAX : std::max(1, str_to_int(column_str)) - 1;
            }

            m_coord = BufferCoord{line, column};
        }
        else if (not only_pos and not params[i].empty() and params[i][0_byte] == '-')
        {
            StringView switch_name = params[i].substr(1_byte);
            auto it = desc.switches.find(switch_name);
            m_state = it == desc.switches.end() and ignore_unknown_switches ?
                        State::Positional : State::Switch;
            if (it == desc.switches.end())
            {
                if (ignore_unknown_switches)
                {
                    m_positional_indices.push_back(i);
                    if (switches_only_at_start)
                        only_pos = true;
                    continue;
                }
                if (ignore_errors)
                    continue;
                throw unknown_option(params[i]);
            }

            auto switch_index = it - desc.switches.begin();
            if (switch_seen[switch_index])
            {
                if (ignore_errors)
                    continue;
                throw runtime_error{format("switch '-{}' specified more than once", it->key)};
            }
            switch_seen[switch_index] = true;

            if (it->value.arg_completer)
            {
               if (++i == params.size())
               {
                   if (ignore_errors)
                       continue;
                   throw missing_option_value(it->key);
               }
               m_state = State::SwitchArgument;
            }

            m_switches[switch_name.str()] = it->value.arg_completer ? params[i] : StringView{};
        }
        else // positional
        {
            m_state = State::Positional;
            if (switches_only_at_start)
                only_pos = true;
            m_positional_indices.push_back(i);
        }
    }
    size_t count = m_positional_indices.size();
    if (not ignore_errors and (count > desc.max_positionals or count < desc.min_positionals))
        throw wrong_argument_count();
}

Optional<StringView> ParametersParser::get_switch(StringView name) const
{
    auto it = m_switches.find(name);
    return it == m_switches.end() ? Optional<StringView>{}
                                  : Optional<StringView>{it->value};
}

}
