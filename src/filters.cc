#include "filters.hh"

#include "buffer.hh"
#include "selection.hh"

namespace Kakoune
{

void preserve_indent(Buffer& buffer, Selection& selection, String& content)
{
    if (content == "\n")
    {
        BufferCoord line_begin{selection.last().line, 0};
        auto first_non_white = buffer.iterator_at(line_begin);
        while ((*first_non_white == '\t' or *first_non_white == ' ') and
               first_non_white != buffer.end())
            ++first_non_white;

        content += buffer.string(line_begin, first_non_white);
    }
}

void cleanup_whitespaces(Buffer& buffer, Selection& selection, String& content)
{
    const auto position = buffer.iterator_at(selection.last());
    if (content[0] == '\n' and position != buffer.begin())
    {
        auto whitespace_start = position-1;
        while ((*whitespace_start == ' ' or *whitespace_start == '\t') and
               whitespace_start != buffer.begin())
            --whitespace_start;
        ++whitespace_start;
        if (whitespace_start != position)
            buffer.erase(whitespace_start, position);
    }
}

void expand_tabulations(Buffer& buffer, Selection& selection, String& content)
{
    const int tabstop = buffer.options()["tabstop"].get<int>();
    if (content == "\t")
    {
        int column = 0;
        const auto position = buffer.iterator_at(selection.last());
        for (auto it = buffer.iterator_at(selection.last().line);
             it != position; ++it)
        {
            kak_assert(*it != '\n');
            if (*it == '\t')
                column += tabstop - (column % tabstop);
            else
               ++column;
        }

        CharCount count = tabstop - (column % tabstop);
        content = String(' ', count);
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
        const auto position = buffer.iterator_at(selection.last());
        auto line_begin = buffer.iterator_at(selection.last().line);
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

                    auto& first = selection.first();
                    auto& last = selection.last();
                    buffer.insert(position, suffix);
                    if (first == last)
                        first = buffer.advance(first, -suffix.length());
                    last = buffer.advance(last, -suffix.length());
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
