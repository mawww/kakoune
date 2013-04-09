#include "filters.hh"
#include "buffer.hh"
#include "selection.hh"

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
    const int tabstop = buffer.options()["tabstop"].get<int>();
    if (content == "\t")
    {
        int column = 0;
        const BufferIterator& position = selection.last();
        for (auto line_it = buffer.iterator_at_line_begin(position);
             line_it != position; ++line_it)
        {
            kak_assert(*line_it != '\n');
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

struct RegexFilter
{
    RegexFilter(const String& line_match, const String& insert_match,
                const String& replacement)
    : m_line_match(line_match.c_str()), m_insert_match(insert_match.c_str()),
      m_replacement(replacement.c_str()) {}

    void operator() (Buffer& buffer, Selection& selection, String& content)
    {
        const auto& position = selection.last();
        auto line_begin = buffer.iterator_at_line_begin(position);
        boost::match_results<BufferIterator> results;
        if (boost::regex_match(content.c_str(), m_insert_match) and
            boost::regex_match(line_begin, position, results, m_line_match))
        {
            content = results.format(m_replacement.c_str());
            auto it = std::find(content.begin(), content.end(), '$');
            if (it != content.end())
            {
                ++it;
                if (it != content.end() && *it == 'c')
                {
                    String suffix(it+1, content.end());
                    content = String(content.begin(), it-1);

                    auto first = selection.first();
                    auto last = selection.last();
                    buffer.insert(position, suffix);
                    if (selection.first() == selection.last())
                        selection.first() -= suffix.length();
                    selection.last() -= suffix.length();
                }
            }
        }
    }

private:
    Regex  m_line_match;
    Regex  m_insert_match;
    String m_replacement;
};

FilterAndId regex_filter_factory(const FilterParameters& params)
{
    if (params.size() != 3)
        throw runtime_error("wrong parameter count");

    return FilterAndId{"re" + params[0] + "__" + params[1],
                       RegexFilter{params[0], params[1], params[2]}};
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

    registry.register_func("preserve_indent", SimpleFilterFactory<preserve_indent>("preserve_indent"));
    registry.register_func("cleanup_whitespaces", SimpleFilterFactory<cleanup_whitespaces>("cleanup_whitespaces"));
    registry.register_func("expand_tabulations", SimpleFilterFactory<expand_tabulations>("expand_tabulations"));
    registry.register_func("regex", regex_filter_factory);
    registry.register_func("group", filter_group_factory);
}

}
