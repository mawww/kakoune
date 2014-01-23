#ifndef parameters_parser_hh_INCLUDED
#define parameters_parser_hh_INCLUDED

#include "exception.hh"
#include "memoryview.hh"
#include "string.hh"

#include <unordered_map>

namespace Kakoune
{

using ParameterList = memoryview<String>;

struct parameter_error : public runtime_error
{
    using runtime_error::runtime_error;
};

struct unknown_option : public parameter_error
{
    unknown_option(const String& name)
        : parameter_error("unknown option '" + name + "'") {}
};

struct missing_option_value: public parameter_error
{
    missing_option_value(const String& name)
        : parameter_error("missing value for option '" + name + "'") {}
};

struct wrong_argument_count : public parameter_error
{
    wrong_argument_count() : parameter_error("wrong argument count") {}
};

using OptionMap = std::unordered_map<String, bool>;

// ParameterParser provides tools to parse command parameters.
// There are 3 types of parameters:
//  * unnamed options, which are accessed by position (ignoring named ones)
//  * named boolean options, which are enabled using '-name' syntax
//  * named string options,  which are defined using '-name value' syntax
struct ParametersParser
{
    enum class Flags
    {
        None = 0,
        OptionsOnlyAtStart = 1,
    };
    friend constexpr Flags operator|(Flags lhs, Flags rhs)
    {
        return (Flags)((int) lhs | (int) rhs);
    }
    friend constexpr bool operator&(Flags lhs, Flags rhs)
    {
        return ((int) lhs & (int) rhs) != 0;
    }

    // the options defines named options, if they map to true, then
    // they are understood as string options, else they are understood as
    // boolean option.
    ParametersParser(const ParameterList& params,
                     OptionMap options,
                     Flags flags = Flags::None,
                     size_t min_positionals = 0,
                     size_t max_positionals = -1);

    // check if a named option (either string or boolean) is specified
    bool has_option(const String& name) const;

    // get a string option value, returns an empty string if the option
    // is not defined
    const String& option_value(const String& name) const;

    // positional parameters count
    size_t positional_count() const;

    struct iterator
    {
    public:
        using value_type = String;
        using pointer = const value_type*;
        using reference = const value_type&;
        using difference_type = size_t;
        using iterator_category = std::forward_iterator_tag;

        iterator(const ParametersParser& parser, size_t index)
            : m_parser(parser), m_index(index) {}

        const String& operator*() const
        {
            return m_parser.m_params[m_parser.m_positional_indices[m_index]];
        }

        const String* operator->() const
        {
            return &m_parser.m_params[m_parser.m_positional_indices[m_index]];
        }

        iterator& operator++() { ++m_index; return *this; }

        bool operator==(const iterator& other) const
        {
            kak_assert(&m_parser == &other.m_parser);
            return m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const
        {
            kak_assert(&m_parser == &other.m_parser);
            return m_index != other.m_index;
        }

        bool operator<(const iterator& other) const
        {
            kak_assert(&m_parser == &other.m_parser);
            return m_index < other.m_index;
        }

    private:
        const ParametersParser& m_parser;
        size_t                  m_index;
    };

    // access positional parameter by index
    const String& operator[] (size_t index) const;
    // positional parameter begin
    iterator begin() const;
    // positional parameter end
    iterator end() const;

private:
    ParameterList     m_params;
    std::vector<size_t> m_positional_indices;
    std::unordered_map<String, bool> m_options;
};

}

#endif // parameters_parser_hh_INCLUDED

