#include "filters.hh"
#include "filter_registry.hh"
#include "buffer.hh"
#include "filter_group.hh"

namespace Kakoune
{

void preserve_indent(Buffer& buffer, Selection& selection, String& content)
{
    if (content == "\n")
    {
         BufferIterator line_begin = buffer.iterator_at_line_begin(selection.last() - 1);
         BufferIterator first_non_white = line_begin;
         while ((*first_non_white == '\t' or *first_non_white == ' ') and
                not first_non_white.is_end())
             ++first_non_white;

         content += buffer.string(line_begin, first_non_white);
    }
}

void cleanup_whitespaces(Buffer& buffer, Selection& selection, String& content)
{
    const BufferIterator& position = selection.last();
    if (content[0] == '\n' and not position.is_begin())
    {
        BufferIterator whitespace_start = position-1;
        while ((*whitespace_start == ' ' or *whitespace_start == '\t') and
               not whitespace_start .is_begin())
            --whitespace_start;
        ++whitespace_start;
        if (whitespace_start!= position)
            buffer.erase(whitespace_start, position);
    }
}

void expand_tabulations(Buffer& buffer, Selection& selection, String& content)
{
    const int tabstop = buffer.option_manager()["tabstop"].as_int();
    if (content == "\t")
    {
        int column = 0;
        const BufferIterator& position = selection.last();
        for (auto line_it = buffer.iterator_at_line_begin(position);
             line_it != position; ++line_it)
        {
            assert(*line_it != '\n');
            if (*line_it == '\t')
                column += tabstop - (column % tabstop);
            else
               ++column;
        }

        int count = tabstop - (column % tabstop);
        content = String();
        for (int i = 0; i < count; ++i)
            content += ' ';
    }
}

template<void (*filter_func)(Buffer&, Selection&, String&)>
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
