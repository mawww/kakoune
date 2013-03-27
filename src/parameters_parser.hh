#ifndef parameters_parser_hh_INCLUDED
#define parameters_parser_hh_INCLUDED

#include "string.hh"
#include "memoryview.hh"
#include "exception.hh"

#include <unordered_map>

namespace Kakoune
{

using ParameterList = memoryview<String>;

struct unknown_option : public runtime_error
{
    unknown_option(const String& name)
        : runtime_error("unknown option '" + name + "'") {}
};

struct missing_option_value: public runtime_error
{
    missing_option_value(const String& name)
        : runtime_error("missing value for option '" + name + "'") {}
};

struct wrong_argument_count : runtime_error
{
    wrong_argument_count() : runtime_error("wrong argument count") {}
};


// ParameterParser provides tools to parse command parameters.
// There are 3 types of parameters:
//  * unnamed options, which are accessed by position (ignoring named ones)
//  * named boolean options, which are enabled using '-name' syntax
//  * named string options,  which are defined using '-name value' syntax
struct ParametersParser
{
    // the options defines named options, if they map to true, then
    // they are understood as string options, else they are understood as
    // boolean option.
    ParametersParser(const ParameterList& params,
                     std::unordered_map<String, bool> options,
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
        typedef String            value_type;
        typedef const value_type* pointer;
        typedef const value_type& reference;
        typedef size_t            difference_type;
        typedef std::forward_iterator_tag iterator_category;

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
            assert(&m_parser == &other.m_parser);
            return m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const
        {
            assert(&m_parser == &other.m_parser);
            return m_index != other.m_index;
        }

        bool operator<(const iterator& other) const
        {
            assert(&m_parser == &other.m_parser);
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

