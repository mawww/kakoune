#include "filters.hh"
#include "filter_registry.hh"
#include "buffer.hh"

namespace Kakoune
{

void preserve_indent(Buffer& buffer, BufferModification& modification)
{
    if (modification.type == BufferModification::Insert and
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

template<void (*filter_func)(Buffer&, BufferModification&)>
class SimpleFilterFactory
{
public:
    SimpleFilterFactory(const std::string& id) : m_id(id) {}

    FilterAndId operator()(Buffer& buffer,
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
}

}
