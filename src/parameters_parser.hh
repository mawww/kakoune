#ifndef parameters_parser_hh_INCLUDED
#define parameters_parser_hh_INCLUDED

#include "exception.hh"
#include "hash_map.hh"
#include "meta.hh"
#include "array_view.hh"
#include "optional.hh"
#include "flags.hh"
#include "string.hh"
#include "string_utils.hh"

namespace Kakoune
{

using ParameterList = ConstArrayView<String>;

struct parameter_error : public runtime_error
{
    using runtime_error::runtime_error;
};

struct unknown_option : public parameter_error
{
    unknown_option(StringView name)
        : parameter_error(format("unknown option '{}'", name)) {}
};

struct missing_option_value: public parameter_error
{
    missing_option_value(StringView name)
        : parameter_error(format("missing value for option '{}'", name)) {}
};

struct wrong_argument_count : public parameter_error
{
    wrong_argument_count() : parameter_error("wrong argument count") {}
};

struct SwitchDesc
{
    bool takes_arg;
    String description;
};

using SwitchMap = HashMap<String, SwitchDesc, MemoryDomain::Commands>;

String generate_switches_doc(const SwitchMap& opts);

struct ParameterDesc
{
    enum class Flags
    {
        None = 0,
        SwitchesOnlyAtStart   = 0b0001,
        SwitchesAsPositional  = 0b0010,
        IgnoreUnknownSwitches = 0b0100
    };
    friend constexpr bool with_bit_ops(Meta::Type<Flags>) { return true; }

    SwitchMap switches;
    Flags flags = Flags::None;
    size_t min_positionals = 0;
    size_t max_positionals = -1;
};

// ParametersParser provides tools to parse command parameters.
// There are 3 types of parameters:
//  * unnamed options, which are accessed by position (ignoring named ones)
//  * named boolean options, which are enabled using '-name' syntax
//  * named string options,  which are defined using '-name value' syntax
struct ParametersParser
{
    // the options defines named options, if they map to true, then
    // they are understood as string options, else they are understood as
    // boolean option.
    ParametersParser(ParameterList params, const ParameterDesc& desc);

    // Return a valid optional if the switch was given, with
    // a non empty StringView value if the switch took an argument.
    Optional<StringView> get_switch(StringView name) const;

    struct iterator
    {
        using difference_type = ptrdiff_t;
        using value_type = String;
        using pointer = String*;
        using reference = String&;
        using iterator_category = std::forward_iterator_tag;

        iterator(const ParametersParser& parser, size_t index)
            : m_parser(parser), m_index(index) {}

        const String& operator*() const { return m_parser[m_index]; }
        const String* operator->() const { return &m_parser[m_index]; }

        iterator& operator++() { ++m_index; return *this; }
        iterator operator++(int) { auto copy = *this; ++m_index; return copy; }

        bool operator==(const iterator& other) const
        {
            kak_assert(&m_parser == &other.m_parser);
            return m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const
        {
            return not (*this == other);
        }

    private:
        const ParametersParser& m_parser;
        size_t                  m_index;
    };

    // positional parameters count
    size_t positional_count() const { return m_positional_indices.size(); }

    // access positional parameter by index
    const String& operator[] (size_t index) const
    {
        kak_assert(index < positional_count());
        return m_params[m_positional_indices[index]];
    }

    ConstArrayView<String> positionals_from(size_t first) const
    {
        // kak_assert(m_desc.flags & (ParameterDesc::Flags::SwitchesOnlyAtStart | ParameterDesc::Flags::SwitchesAsPositional));
        return m_params.subrange(first < m_positional_indices.size() ? m_positional_indices[first] : -1);
    }

    iterator begin() const { return iterator(*this, 0); }
    iterator end() const { return iterator(*this, m_positional_indices.size()); }

private:
    ParameterList m_params;
    Vector<size_t, MemoryDomain::Commands> m_positional_indices;
    HashMap<String, StringView> m_switches;
};

}

#endif // parameters_parser_hh_INCLUDED
