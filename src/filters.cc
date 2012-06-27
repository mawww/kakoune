#include "filters.hh"
#include "filter_registry.hh"
#include "buffer.hh"
#include "filter_group.hh"

namespace Kakoune
{

void preserve_indent(Buffer& buffer, Modification& modification)
{
    if (modification.type == Modification::Insert and
        modification.content == "\n")
    {
         BufferIterator line_begin = buffer.iterator_at_line_begin(modification.position - 1);
         BufferIterator first_non_white = line_begin;
         while ((*first_non_white == '\t' or *first_non_white == ' ') and
                not first_non_white.is_end())
             ++first_non_white;

         modification.content += buffer.string(line_begin, first_non_white);
    }
}

void cleanup_whitespaces(Buffer& buffer, Modification& modification)
{
    if (modification.type == Modification::Insert and
        modification.content[0] == '\n' and not modification.position.is_begin())
    {
        BufferIterator position = modification.position-1;
        while ((*position == ' ' or *position == '\t') and not position.is_begin())
            --position;
        ++position;
        if (position != modification.position)
        {
            buffer.modify(Modification::make_erase(position, modification.position));
            modification.position = position;
        }
    }
}

void expand_tabulations(Buffer& buffer, Modification& modification)
{
    const int tabstop = buffer.option_manager()["tabstop"].as_int();
    if (modification.type == Modification::Insert and
        modification.content == "\t")
    {
        int column = 0;
        BufferCoord pos = buffer.line_and_column_at(modification.position);
        for (auto line_it = buffer.iterator_at({pos.line, 0});
             line_it != modification.position; ++line_it)
        {
            assert(*line_it != '\n');
            if (*line_it == '\t')
                column += tabstop - (column % tabstop);
            else
               ++column;
        }

        int count = tabstop - (column % tabstop);
        modification.content = String();
        for (int i = 0; i < count; ++i)
            modification.content += ' ';
    }
}

template<void (*filter_func)(Buffer&, Modification&)>
class SimpleFilterFactory
{
public:
    SimpleFilterFactory(const String& id) : m_id(id) {}

    FilterAndId operator()(const FilterParameters& params) const
    {
        return FilterAndId(m_id, FilterFunc(filter_func));
    }
private:
    String m_id;
};

FilterAndId filter_group_factory(const FilterParameters& params)
{
    if (params.size() != 1)
        throw runtime_error("wrong parameter count");

    return FilterAndId(params[0], FilterGroup());
}

void register_filters()
{
    FilterRegistry& registry = FilterRegistry::instance();

    registry.register_factory("preserve_indent", SimpleFilterFactory<preserve_indent>("preserve_indent"));
    registry.register_factory("cleanup_whitespaces", SimpleFilterFactory<cleanup_whitespaces>("cleanup_whitespaces"));
    registry.register_factory("expand_tabulations", SimpleFilterFactory<expand_tabulations>("expand_tabulations"));
    registry.register_factory("group", filter_group_factory);
}

}
