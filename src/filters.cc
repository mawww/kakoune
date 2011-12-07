#include "filters.hh"
#include "filter_registry.hh"
#include "buffer.hh"

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

void expand_tabulations(Buffer& buffer, Modification& modification)
{
    const int tabstop = 8;
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
        modification.content = std::string(count, ' ');
    }
}

template<void (*filter_func)(Buffer&, Modification&)>
class SimpleFilterFactory
{
public:
    SimpleFilterFactory(const std::string& id) : m_id(id) {}

    FilterAndId operator()(Window& window,
                           const FilterParameters& params) const
    {
        return FilterAndId(m_id, FilterFunc(filter_func));
    }
private:
    std::string m_id;
};

void register_filters()
{
    FilterRegistry& registry = FilterRegistry::instance();

    registry.register_factory("preserve_indent", SimpleFilterFactory<preserve_indent>("preserve_indent"));
    registry.register_factory("expand_tabulations", SimpleFilterFactory<expand_tabulations>("expand_tabulations"));
}

}
