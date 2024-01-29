#include "tree_sitter.hh"

#ifdef KAK_TREE_SITTER

#include "buffer.hh"
#include "changes.hh"
#include "context.hh"
#include "coord.hh"
#include "exception.hh"
#include "face.hh"
#include "face_registry.hh"
#include "file.hh"

#include <dlfcn.h>
#include <tree_sitter/api.h>

namespace Kakoune
{

using DlPtr = std::unique_ptr<void, decltype([](void* ptr) {
    dlclose(ptr); })>;
using TSParserPtr = std::unique_ptr<TSParser, decltype([](TSParser* ptr) {
    ts_parser_delete(ptr); })>;
using TSTreePtr = std::unique_ptr<TSTree, decltype([](TSTree* ptr) {
    ts_tree_delete(ptr); })>;
using TSQueryPtr = std::unique_ptr<TSQuery, decltype([](TSQuery* ptr) {
    ts_query_delete(ptr); })>;
using TSQueryCursorPtr = std::unique_ptr<TSQueryCursor, decltype([](TSQueryCursor* ptr) {
    ts_query_cursor_delete(ptr); })>;

static TSPoint coord_to_point(BufferCoord coord)
{
    return {
        .row = static_cast<uint32_t>((int) coord.line),
        .column = static_cast<uint32_t>((int) coord.column) };
}

static BufferCoord point_to_coord(TSPoint point)
{
    return { static_cast<int>(point.row), static_cast<int>(point.column) };
}

static void highlight_node(DisplayBuffer& display_buffer, TSNode const& node, Face const& face)
{
    auto begin = point_to_coord(ts_node_start_point(node));
    auto end = point_to_coord(ts_node_end_point(node));

    for (auto& line : display_buffer.lines())
    {
        auto& range = line.range();
        if (range.end <= begin or end < range.begin)
            continue;

        for (auto atom_it = line.begin(); atom_it != line.end(); ++atom_it)
        {
            bool is_replaced = atom_it->type() == DisplayAtom::ReplacedRange;

            if (not atom_it->has_buffer_range() or
                end <= atom_it->begin() or begin >= atom_it->end())
                continue;

            if (not is_replaced and begin > atom_it->begin())
                atom_it = ++line.split(atom_it, begin);

            if (not is_replaced and end < atom_it->end())
            {
                atom_it = line.split(atom_it, end);
                atom_it->face = merge_faces(atom_it->face, face);
                ++atom_it;
            }
            else
                atom_it->face = merge_faces(atom_it->face, face);
        }
    }
}

struct InjectionHighlighterApplier
{
    DisplayBuffer& display_buffer;
    HighlightContext& context;
    DisplayLineList::iterator cur_line = display_buffer.lines().begin();
    DisplayLineList::iterator end_line = display_buffer.lines().end();
    DisplayLine::iterator cur_atom = cur_line->begin();
    DisplayBuffer region_display{};

    void operator()(BufferCoord begin, BufferCoord end, Highlighter& highlighter)
    {
        if (begin == end)
            return;

        auto first_line = std::find_if(cur_line, end_line, [&](auto&& line) { return line.range().end > begin; });
        if (first_line != cur_line and first_line != end_line)
            cur_atom = first_line->begin();
        cur_line = first_line;
        if (cur_line == end_line or cur_line->range().begin >= end)
            return;

        auto& region_lines = region_display.lines();
        region_lines.clear();
        Vector<std::pair<DisplayLineList::iterator, size_t>> insert_pos;
        while (cur_line != end_line and cur_line->range().begin < end)
        {
            auto& line = *cur_line;
            auto first = std::find_if(cur_atom, line.end(), [&](auto&& atom) { return atom.has_buffer_range() and atom.end() > begin; });
            if (first != line.end() and first->type() == DisplayAtom::Range and first->begin() < begin)
                first = ++line.split(first, begin);
            auto idx = first - line.begin();

            auto last = std::find_if(first, line.end(), [&](auto&& atom) { return atom.has_buffer_range() and atom.end() > end; });
            if (last != line.end() and last->type() == DisplayAtom::Range and last->begin() < end)
                last = ++line.split(last, end);

            if (line.begin() + idx != last)
            {
                insert_pos.emplace_back(cur_line, idx);
                region_lines.push_back(line.extract(line.begin() + idx, last));
            }

            if (idx != line.atoms().size())
                break;
            else if (++cur_line != end_line)
                cur_atom = cur_line->begin();
        }

        if (region_lines.empty())
            return;

        region_display.compute_range();
        highlighter.highlight(context, region_display, {begin, end});

        for (size_t i = 0; i < insert_pos.size(); ++i)
        {
            auto& [line_it, idx] = insert_pos[i];
            auto& atoms = region_lines[i].atoms();
            auto it = line_it->insert(
                line_it->begin() + idx,
                std::move_iterator(atoms.begin()),
                std::move_iterator(atoms.end()));

            if (line_it == cur_line)
                cur_atom = it + atoms.size();
        }
    }
};

struct TreeSitterInjectionHighlighter : public HighlighterDelegate
{
    TreeSitterInjectionHighlighter(std::unique_ptr<Highlighter>&& delegate)
        : HighlighterDelegate{delegate->passes()}
        , m_delegate{std::move(delegate)}
   {
   }

    bool has_children() const override
    {
        return m_delegate->has_children();
    }

    Highlighter& get_child(StringView path) override
    {
        return m_delegate->get_child(path);
    }

    void add_child(String name, std::unique_ptr<Highlighter>&& hl, bool override) override
    {
        return m_delegate->add_child(name, std::move(hl), override);
    }

    void remove_child(StringView id) override
    {
        return m_delegate->remove_child(id);
    }

    Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const override
    {
        return m_delegate->complete_child(path, cursor_pos, group);
    }

    void fill_unique_ids(Vector<StringView>& unique_ids) const override
    {
        return m_delegate->fill_unique_ids(unique_ids);
    }

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
    {
        return m_delegate->highlight(context, display_buffer, range);
    }

    Highlighter& delegate() const override
    {
        return *m_delegate;
    }

private:
    std::unique_ptr<Highlighter> m_delegate;
};

struct TreeSitterHighlighter : public Highlighter
{
public:
    using InjectionsMap = HashMap<String, std::unique_ptr<TreeSitterInjectionHighlighter>, MemoryDomain::Highlight>;
    using FacesSpec = Vector<std::pair<String, FaceSpec>, MemoryDomain::Highlight>;

    TSTreePtr parse(Buffer& buffer, BufferRange range)
    {
        auto start_byte = buffer.distance({0, 0}, range.begin);
        auto end_byte = start_byte + buffer.distance(range.begin, range.end);

        TSRange ts_range = {
            .start_point = coord_to_point(range.begin),
            .end_point = coord_to_point(range.end),
            .start_byte = static_cast<uint32_t>(int{start_byte}),
            .end_byte = static_cast<uint32_t>(int{end_byte}),
        };
        ts_parser_set_included_ranges(m_parser.get(), &ts_range, 1);

        auto read = [](void* ptr, uint32_t, TSPoint point, uint32_t *bytes_read)
        {
            auto& buffer = *reinterpret_cast<Buffer*>(ptr);

            auto coord = point_to_coord(point);
            if (coord.line >= buffer.line_count() or
                coord.column >= buffer[coord.line].length()) {
                *bytes_read = 0;
                return "";
            }

            auto string = buffer[coord.line].substr(coord.column);
            *bytes_read = static_cast<uint32_t>(int{string.length()});
            return string.data();
        };

        auto tree = TSTreePtr{ts_parser_parse(m_parser.get(), nullptr, {
            .payload = reinterpret_cast<void*>(&buffer),
            .read = read,
            .encoding = TSInputEncodingUTF8 })};

        if (not tree)
            ts_parser_reset(m_parser.get());

        return tree;
    }

    void execute_queries(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range, TSTree* tree)
    {
        uint32_t length;

        if (not tree)
            return;

        auto display_range = display_buffer.range();
        auto root = ts_tree_root_node(tree);
        ts_query_cursor_set_point_range(
            m_cursor.get(),
            coord_to_point(display_range.begin),
            coord_to_point(display_range.end));

        if (m_highlights_query) {
            ts_query_cursor_exec(m_cursor.get(), m_highlights_query.get(), root);
            TSQueryMatch match;
            uint32_t capture_index;
            while (ts_query_cursor_next_capture(m_cursor.get(), &match, &capture_index)) {
                auto& capture = match.captures[capture_index];
                char const* data = ts_query_capture_name_for_id(m_highlights_query.get(),
                    capture.index, &length);

                auto name = StringView{data, static_cast<int>(length)};
                auto it = find_if(m_faces,
                     [name](const auto& pair)
                     { return name.starts_with(pair.first); });

                if (it == m_faces.end())
                    continue;

                auto&[id, spec] = *it;
                highlight_node(display_buffer, capture.node, context.context.faces()[spec]);
            }
        }

        if (m_injections_query) {
            TSQueryMatch match;
            InjectionHighlighterApplier apply_highlighter{display_buffer, context};
            ts_query_cursor_exec(m_cursor.get(), m_injections_query.get(), root);
            while (ts_query_cursor_next_match(m_cursor.get(), &match)) {
                Optional<String> language{};
                Optional<BufferRange> content{};

                for (uint16_t capture_index = 0; capture_index < match.capture_count; ++capture_index) {
                    auto& capture = match.captures[capture_index];
                    char const* data = ts_query_capture_name_for_id(m_injections_query.get(),
                        capture.index, &length);

                    auto capture_name = StringView{data, static_cast<int>(length)};
                    auto capture_begin = point_to_coord(ts_node_start_point(capture.node));
                    auto capture_end = point_to_coord(ts_node_end_point(capture.node));

                    if (capture_name == "injection.language")
                        language = context.context.buffer().string(capture_begin, capture_end);
                    else if (capture_name == "injection.content")
                        content = {{capture_begin, capture_end}};
                }

                if (not language or not content)
                    continue;

                if (m_injections.contains(*language))
                    apply_highlighter(content->begin, content->end,
                                      m_injections[*language]->delegate());
            }
        }
    }

    TreeSitterHighlighter(
        DlPtr lib,
        TSParserPtr parser,
        TSQueryPtr highlights,
        TSQueryPtr injections,
        FacesSpec faces)
        : Highlighter(HighlightPass::Colorize)
        , m_lib{std::move(lib)}
        , m_parser{std::move(parser)}
        , m_highlights_query{std::move(highlights)}
        , m_injections_query{std::move(injections)}
        , m_faces{std::move(faces)}
        , m_cursor{ts_query_cursor_new()}
    {
        std::sort(m_faces.begin(), m_faces.end(),
                  [](auto&& lhs, auto&& rhs) { return lhs.first > rhs.first; });
    }

    void do_highlight(HighlightContext context, DisplayBuffer& display_buffer, BufferRange range) override
    {
        auto tree = parse(context.context.buffer(), range);
        execute_queries(context, display_buffer, range, tree.get());
    }

    bool has_children() const override { return true; }

    Highlighter& get_child_impl(StringView path) const
    {
        auto sep_it = find(path, '/');
        StringView id(path.begin(), sep_it);
        auto it = m_injections.find(id);
        if (it == m_injections.end())
            throw child_not_found(format("no such id: {}", id));
        if (sep_it == path.end())
            return *it->value;
        else
            return it->value->get_child({sep_it+1, path.end()});
    }

    Highlighter& get_child(StringView path) override
    {
        return get_child_impl(path);
    }

    void add_child(String name, std::unique_ptr<Highlighter>&& hl, bool override) override
    {
        if (not dynamic_cast<TreeSitterInjectionHighlighter*>(hl.get()))
            throw runtime_error{"only tree-sitter-injection highlighter can be added as child of a tree-sitter highlighter"};
        auto it = m_injections.find(name);
        if (not override and it != m_injections.end())
            throw runtime_error{format("duplicate id: '{}'", name)};

        std::unique_ptr<TreeSitterInjectionHighlighter> injection_hl{dynamic_cast<TreeSitterInjectionHighlighter*>(hl.release())};
        if (it != m_injections.end())
            it->value = std::move(injection_hl);
        else
            m_injections.insert({std::move(name), std::move(injection_hl)});
    }

    void remove_child(StringView id) override
    {
        m_injections.remove(id);
    }

    Completions complete_child(StringView path, ByteCount cursor_pos, bool group) const override
    {
        auto sep_it = find(path, '/');
        if (sep_it != path.end())
        {
            ByteCount offset = sep_it+1 - path.begin();
            Highlighter& hl = get_child_impl({path.begin(), sep_it});
            return offset_pos(hl.complete_child(path.substr(offset), cursor_pos - offset, group), offset);
        }

        auto container = m_injections | transform(&decltype(m_injections)::Item::key);
        auto completions_flags = group ? Completions::Flags::None : Completions::Flags::Menu;
        return { 0, 0, complete(path, cursor_pos, container), completions_flags };
    }

    static bool is_tree_sitter(Highlighter* parent)
    {
        if (dynamic_cast<TreeSitterHighlighter*>(parent))
            return true;
        if (auto* highlighter = dynamic_cast<HighlighterDelegate*>(parent))
            return is_tree_sitter(&highlighter->delegate());
        return false;
    }

    static std::unique_ptr<Highlighter> create(HighlighterParameters params, Highlighter*)
    {
        uint32_t error_offset;
        TSQueryError error_type;

        if (params.size() < 2)
            throw runtime_error{"wrong parameter count"};

        auto lang = params[0];
        auto dir = String{params[1]};
        auto parser_path = dir + "/parser";
        auto highlights_query_path = dir + "/queries/highlights.scm";
        auto injections_query_path = dir + "/queries/injections.scm";

        auto lib = DlPtr{dlopen(parser_path.c_str(), RTLD_LAZY | RTLD_LOCAL)};
        if (not lib)
            throw runtime_error{format(
                "could not load {} parser at {}",
                lang, dlerror())};

        auto sym = String{"tree_sitter_"};
        sym += lang;

        auto get_language = reinterpret_cast<TSLanguage*(*)()>(dlsym(lib.get(), sym.c_str()));
        if (not get_language)
            throw runtime_error{format(
                "could not load {} parser at {}",
                lang, dlerror())};

        auto language = get_language();
        auto parser = TSParserPtr{ts_parser_new()};
        if (not ts_parser_set_language(parser.get(), language))
            throw runtime_error{format(
                "could not load {} parser at {}: incompatible ABI version {}, expected at least {}",
                lang, dir, ts_language_version(language),
                TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION)};

        TSQueryPtr highlights_query{nullptr};
        if (file_exists(highlights_query_path))
        {
            auto highlights_query_str = read_file(highlights_query_path);
            highlights_query = TSQueryPtr{ts_query_new(
                language,
                highlights_query_str.data(),
                int{highlights_query_str.length()},
                &error_offset,
                &error_type)};
        }

        TSQueryPtr injections_query{nullptr};
        if (file_exists(injections_query_path))
        {
            auto injections_query_str = read_file(injections_query_path);
            injections_query = TSQueryPtr{ts_query_new(
                language,
                injections_query_str.data(),
                int{injections_query_str.length()},
                &error_offset,
                &error_type)};
        }

        if (not highlights_query and not injections_query)
            throw runtime_error{format(
                "could not load {} parser at {}: missing queries for parser",
                lang, dir)};

        FacesSpec faces;
        for (auto& spec : params.subrange(2))
        {
            auto colon = find(spec, ':');
            if (colon == spec.end())
                throw runtime_error(format("wrong face spec: '{}' expected <id>:<face>", spec));
            StringView id{spec.begin(), colon};
            StringView face{colon+1, spec.end()};
            faces.emplace_back(id, parse_face(face));
        }

        return std::make_unique<TreeSitterHighlighter>(
            std::move(lib),
            std::move(parser),
            std::move(highlights_query),
            std::move(injections_query),
            std::move(faces));
    }

    static std::unique_ptr<Highlighter> create_injection(HighlighterParameters params, Highlighter* parent)
    {
        if (not is_tree_sitter(parent))
            throw runtime_error{"tree-sitter-injection highlighter can only be added to a tree-sitter parent"};

        if (params.empty())
            throw runtime_error{"wrong parameter count"};

        const auto& type = params[0];
        auto& registry = HighlighterRegistry::instance();
        auto it = registry.find(type);
        if (it == registry.end())
            throw runtime_error(format("no such highlighter type: '{}'", type));

        auto delegate = it->value.factory(params.subrange(1), nullptr);
        return std::make_unique<TreeSitterInjectionHighlighter>(std::move(delegate));
    }

private:
    DlPtr m_lib;
    TSParserPtr m_parser;
    TSQueryPtr m_highlights_query;
    TSQueryPtr m_injections_query;
    FacesSpec m_faces;
    TSQueryCursorPtr m_cursor;
    InjectionsMap m_injections{};
};

std::unique_ptr<Highlighter> create_tree_sitter_highlighter(HighlighterParameters params, Highlighter* parent) {
    return TreeSitterHighlighter::create(params, parent);
}

std::unique_ptr<Highlighter> create_tree_sitter_injection_highlighter(HighlighterParameters params, Highlighter* parent) {
    return TreeSitterHighlighter::create_injection(params, parent);
}

}

#else // KAK_TREE_SITTER

#include "exception.hh"

namespace Kakoune {

std::unique_ptr<Highlighter> create_tree_sitter_highlighter(HighlighterParameters params, Highlighter* parent) {
    throw runtime_error{"This binary was compiled without tree-sitter support"};
}

std::unique_ptr<Highlighter> create_tree_sitter_injection_highlighter(HighlighterParameters params, Highlighter* parent) {
    throw runtime_error{"This binary was compiled without tree-sitter support"};
}

}

#endif // KAK_TREE_SITTER
