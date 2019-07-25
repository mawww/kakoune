#include "regex_impl.hh"

#include "exception.hh"
#include "string.hh"
#include "unicode.hh"
#include "unit_tests.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "string_utils.hh"
#include "vector.hh"
#include "utils.hh"

#include <cstdio>
#include <cstring>

namespace Kakoune
{

constexpr Codepoint CompiledRegex::StartDesc::other;
constexpr Codepoint CompiledRegex::StartDesc::count;

struct ParsedRegex
{
    enum Op : char
    {
        Literal,
        AnyChar,
        AnyCharExceptNewLine,
        Class,
        CharacterType,
        Sequence,
        Alternation,
        LineStart,
        LineEnd,
        WordBoundary,
        NotWordBoundary,
        SubjectBegin,
        SubjectEnd,
        ResetStart,
        LookAhead,
        NegativeLookAhead,
        LookBehind,
        NegativeLookBehind,
    };

    struct Quantifier
    {
        enum Type : char
        {
            One,
            Optional,
            RepeatZeroOrMore,
            RepeatOneOrMore,
            RepeatMinMax,
        };
        Type type = One;
        bool greedy = true;
        int16_t min = -1, max = -1;

        bool allows_none() const
        {
            return type == Quantifier::Optional or
                   type == Quantifier::RepeatZeroOrMore or
                  (type == Quantifier::RepeatMinMax and min <= 0);
        }

        bool allows_infinite_repeat() const
        {
            return type == Quantifier::RepeatZeroOrMore or
                   type == Quantifier::RepeatOneOrMore or
                  (type == Quantifier::RepeatMinMax and max < 0);
        };
    };

    using NodeIndex = uint16_t;
    struct Node
    {
        Op op;
        bool ignore_case;
        NodeIndex children_end;
        Codepoint value;
        Quantifier quantifier;
    };
    static_assert(sizeof(Node) == 16, "");

    Vector<Node, MemoryDomain::Regex> nodes;

    Vector<CharacterClass, MemoryDomain::Regex> character_classes;
    Vector<CompiledRegex::NamedCapture, MemoryDomain::Regex> named_captures;
    uint32_t capture_count;
};

namespace
{

template<RegexMode mode = RegexMode::Forward>
struct Children
{
    static_assert(has_direction(mode));
    using Index = ParsedRegex::NodeIndex;
    struct Sentinel {};
    struct Iterator
    {
        static constexpr bool forward = mode & RegexMode::Forward;
        Iterator(ArrayView<const ParsedRegex::Node> nodes, Index index)
          : m_nodes{nodes},
            m_pos(forward ? index+1 : find_prev(index, nodes[index].children_end)),
            m_end(forward ? nodes[index].children_end : index)
        {}

        Iterator& operator++()
        {
            m_pos = forward ? m_nodes[m_pos].children_end : find_prev(m_end, m_pos);
            return *this;
        }

        Index operator*() const { return m_pos; }
        bool operator!=(Sentinel) const { return m_pos != m_end; }

        Index find_prev(Index parent, Index pos) const
        {
            Index child = parent+1;
            if (child == pos)
                return parent;
            while (m_nodes[child].children_end != pos)
                child = m_nodes[child].children_end;
            return child;
        }

        ArrayView<const ParsedRegex::Node> m_nodes;
        Index m_pos;
        Index m_end;
    };

    Iterator begin() const { return {m_parsed_regex.nodes, m_index}; }
    Sentinel end() const { return {}; }

    const ParsedRegex& m_parsed_regex;
    const Index m_index;
};

}

// Recursive descent parser based on naming used in the ECMAScript
// standard, although the syntax is not fully compatible.
struct RegexParser
{
    RegexParser(StringView re)
        : m_regex{re}, m_pos{re.begin(), re}
    {
        m_parsed_regex.capture_count = 1;
        m_parsed_regex.nodes.reserve((size_t)re.length());
        NodeIndex root = disjunction(0);
        kak_assert(root == 0);
    }

    ParsedRegex get_parsed_regex() { return std::move(m_parsed_regex); }

    static ParsedRegex parse(StringView re) { return RegexParser{re}.get_parsed_regex(); }

private:
    struct InvalidPolicy
    {
        Codepoint operator()(Codepoint cp) { throw regex_error{"Invalid utf8 in regex"}; }
    };

    enum class Flags
    {
        None              = 0,
        IgnoreCase        = 1 << 0,
        DotMatchesNewLine = 1 << 1,
    };
    friend constexpr bool with_bit_ops(Meta::Type<Flags>) { return true; }

    using Iterator = utf8::iterator<const char*, const char*, Codepoint, int, InvalidPolicy>;
    using NodeIndex = ParsedRegex::NodeIndex;

    NodeIndex disjunction(uint32_t capture = -1)
    {
        NodeIndex index = new_node(ParsedRegex::Alternation);
        get_node(index).value = capture;
        while (true)
        {
            alternative();
            if (at_end() or *m_pos != '|')
                break;
            ++m_pos;
        }
        get_node(index).children_end = m_parsed_regex.nodes.size();

        return index;
    }

    NodeIndex alternative(ParsedRegex::Op op = ParsedRegex::Sequence)
    {
        NodeIndex index = new_node(op);
        while (auto t = term())
        {}
        get_node(index).children_end = m_parsed_regex.nodes.size();

        return index;
    }

    Optional<NodeIndex> term()
    {
        while (modifiers()) // read all modifiers
        {}
        if (auto node = assertion())
            return node;
        if (auto node = atom())
        {
            get_node(*node).quantifier = quantifier();
            return node;
        }
        return {};
    }

    bool modifiers()
    {
        auto it = m_pos.base();
        if (m_regex.end() - it >= 4 and *it++ == '(' and *it++ == '?')
        {
            while (true)
            {
                auto m = *it++;
                switch (m)
                {
                    case 'i': m_flags |= Flags::IgnoreCase; break;
                    case 'I': m_flags &= ~Flags::IgnoreCase; break;
                    case 's': m_flags |= Flags::DotMatchesNewLine; break;
                    case 'S': m_flags &= ~Flags::DotMatchesNewLine; break;
                    case ')':
                        m_pos = Iterator{it, m_regex};
                        return true;
                    default: return false;
                }
            }
        }
        return false;
    }

    Optional<NodeIndex> assertion()
    {
        if (at_end())
            return {};

        switch (*m_pos)
        {
            case '^': ++m_pos; return new_node(ParsedRegex::LineStart);
            case '$': ++m_pos; return new_node(ParsedRegex::LineEnd);
            case '\\':
                if (m_pos+1 == m_regex.end())
                    return {};
                switch (*(m_pos+1))
                {
                    case 'b': m_pos += 2; return new_node(ParsedRegex::WordBoundary);
                    case 'B': m_pos += 2; return new_node(ParsedRegex::NotWordBoundary);
                    case 'A': m_pos += 2; return new_node(ParsedRegex::SubjectBegin);
                    case 'z': m_pos += 2; return new_node(ParsedRegex::SubjectEnd);
                    case 'K': m_pos += 2; return new_node(ParsedRegex::ResetStart);
                }
                break;
            case '(':
            {
                auto it = m_pos.base()+1;
                if (m_regex.end() - it <= 2 or *it++ != '?')
                    return {};

                ParsedRegex::Op op;
                switch (*it++)
                {
                    case '=': op = ParsedRegex::LookAhead; break;
                    case '!': op = ParsedRegex::NegativeLookAhead; break;
                    case '<':
                    {
                        switch (*it++)
                        {
                            case '=': op = ParsedRegex::LookBehind; break;
                            case '!': op = ParsedRegex::NegativeLookBehind; break;
                            default: return {};
                        }
                        break;
                    }
                    default: return {};
                }
                m_pos = Iterator{it, m_regex};
                NodeIndex lookaround = alternative(op);
                if (at_end() or *m_pos++ != ')')
                    parse_error("unclosed parenthesis");

                validate_lookaround(lookaround);
                return lookaround;
            }
        }
        return {};
    }

    Optional<NodeIndex> atom()
    {
        if (at_end())
            return {};

        switch (const Codepoint cp = *m_pos)
        {
            case '.':
                ++m_pos;
                if (m_flags & Flags::DotMatchesNewLine)
                    return new_node(ParsedRegex::AnyChar);
                else
                    return new_node(ParsedRegex::AnyCharExceptNewLine);
            case '(':
            {
                uint32_t capture_group = -1;
                const char* it = (++m_pos).base();
                if (m_regex.end() - it < 2 or *it++ != '?')
                    capture_group = m_parsed_regex.capture_count++;
                else if (*it == ':')
                    m_pos = Iterator{++it, m_regex};
                else if (*it == '<')
                {
                    const auto name_start = ++it;
                    while (it != m_regex.end() and is_word(*it))
                        ++it;
                    if (it == m_regex.end() or *it != '>')
                        parse_error("named captures should be only ascii word characters");
                    capture_group = m_parsed_regex.capture_count++;
                    m_parsed_regex.named_captures.push_back({{name_start, it}, capture_group});
                    m_pos = Iterator{++it, m_regex};
                }

                NodeIndex content = disjunction(capture_group);
                if (at_end() or *m_pos++ != ')')
                    parse_error("unclosed parenthesis");
                return content;
            }
            case '\\':
                ++m_pos;
                return atom_escape();
            case '[':
                ++m_pos;
                return character_class();
            case '|': case ')':
                return {};
            default:
                if (contains("^$.*+?[]{}", cp) or (cp >= 0xF0000 and cp <= 0xFFFFF))
                    parse_error(format("unexpected '{}'", cp));
                ++m_pos;
                return new_node(ParsedRegex::Literal, cp);
        }
    }

    NodeIndex atom_escape()
    {
        const Codepoint cp = *m_pos++;

        if (cp == 'Q')
        {
            auto escaped_sequence = new_node(ParsedRegex::Sequence);
            constexpr StringView end_mark{"\\E"};

            auto quote_end = std::search(m_pos.base(), m_regex.end(), end_mark.begin(), end_mark.end());
            while (m_pos != quote_end)
                new_node(ParsedRegex::Literal, *m_pos++);
            get_node(escaped_sequence).children_end = m_parsed_regex.nodes.size();

            if (quote_end != m_regex.end())
                m_pos += 2;

            return escaped_sequence;
        }

        // CharacterClassEscape
        auto class_it = find_if(character_class_escapes, [cp](auto& c) { return c.cp == cp; });
        if (class_it != std::end(character_class_escapes))
            return new_node(ParsedRegex::CharacterType, (Codepoint)class_it->ctype);

        // CharacterEscape
        for (auto& control : control_escapes)
        {
            if (control.name == cp)
                return new_node(ParsedRegex::Literal, control.value);
        }

        auto read_hex = [this](size_t count)
        {
            Codepoint res = 0;
            for (int i = 0; i < count; ++i)
            {
                if (at_end())
                    parse_error("unterminated hex sequence");
                Codepoint digit = *m_pos++;
                Codepoint digit_value;
                if ('0' <= digit and digit <= '9')
                    digit_value = digit - '0';
                else if ('a' <= digit and digit <= 'f')
                    digit_value = 0xa + digit - 'a';
                else if ('A' <= digit and digit <= 'F')
                    digit_value = 0xa + digit - 'A';
                else
                    parse_error(format("invalid hex digit '{}'", digit));

                res = res * 16 + digit_value;
            }
            return res;
        };

        if (cp == '0')
            return new_node(ParsedRegex::Literal, '\0');
        else if (cp == 'c')
        {
            if (at_end())
                parse_error("unterminated control escape");
            Codepoint ctrl = *m_pos++;
            if (('a' <= ctrl and ctrl <= 'z') or ('A' <= ctrl and ctrl <= 'Z'))
                return new_node(ParsedRegex::Literal, ctrl % 32);
            parse_error(format("Invalid control escape character '{}'", ctrl));
        }
        else if (cp == 'x')
            return new_node(ParsedRegex::Literal, read_hex(2));
        else if (cp == 'u')
            return new_node(ParsedRegex::Literal, read_hex(4));

        if (contains("^$\\.*+?()[]{}|", cp)) // SyntaxCharacter
            return new_node(ParsedRegex::Literal, cp);
        parse_error(format("unknown atom escape '{}'", cp));
    }

    void normalize_ranges(Vector<CharacterClass::Range, MemoryDomain::Regex>& ranges)
    {
        if (ranges.empty())
            return;

        // Sort ranges so that we can use binary search
        std::sort(ranges.begin(), ranges.end(),
                  [](auto& lhs, auto& rhs) { return lhs.min < rhs.min; });

        // merge overlapping ranges
        auto pos = ranges.begin();
        for (auto next = pos+1; next != ranges.end(); ++next)
        {
            if (pos->max + 1 >= next->min)
            {
                if (next->max > pos->max)
                    pos->max = next->max;
            }
            else
                *++pos = *next;
        }
        ranges.erase(pos+1, ranges.end());
    }

    NodeIndex character_class()
    {
        CharacterClass character_class;

        character_class.ignore_case = (m_flags & Flags::IgnoreCase);
        character_class.negative = m_pos != m_regex.end() and *m_pos == '^';
        if (character_class.negative)
            ++m_pos;

        while (m_pos != m_regex.end() and *m_pos != ']')
        {
            auto cp = *m_pos++;
            if (cp == '-')
            {
                character_class.ranges.push_back({ '-', '-' });
                continue;
            }

            if (at_end())
                break;

            if (cp == '\\')
            {
                auto it = find_if(character_class_escapes,
                                  [cp = *m_pos](auto&& t) { return t.cp == cp; });
                if (it != std::end(character_class_escapes))
                {
                    character_class.ctypes |= it->ctype;
                    ++m_pos;
                    continue;
                }
                else // its an escaped character
                {
                    cp = *m_pos++;
                    auto it = find_if(control_escapes, [cp](auto&& t) { return t.name == cp; });
                    if (it != std::end(control_escapes))
                        cp = it->value;
                    else if (not contains("^$\\.*+?()[]{}|-", cp)) // SyntaxCharacter and -
                        parse_error(format("unknown character class escape '{}'", cp));
                }
            }

            CharacterClass::Range range = { cp, cp };
            if (*m_pos == '-')
            {
                if (++m_pos == m_regex.end())
                    break;
                if (*m_pos != ']')
                {
                    range.max = *m_pos++;
                    if (range.min > range.max)
                        parse_error("invalid range specified");
                }
                else
                {
                    character_class.ranges.push_back(range);
                    range = { '-', '-' };
                }
            }
            character_class.ranges.push_back(range);
        }
        if (at_end())
            parse_error("unclosed character class");
        ++m_pos;

        if (character_class.ignore_case)
        {
            for (auto& range : character_class.ranges)
            {
                range.min = to_lower(range.min);
                range.max = to_lower(range.max);
            }
        }

        normalize_ranges(character_class.ranges);

        // Optimize the relatively common case of using a character class to
        // escape a character, such as [*]
        if (character_class.ctypes == CharacterType::None and not character_class.negative and
            character_class.ranges.size() == 1 and
            character_class.ranges.front().min == character_class.ranges.front().max)
            return new_node(ParsedRegex::Literal, character_class.ranges.front().min);

        if (character_class.ctypes != CharacterType::None and not character_class.negative and
            character_class.ranges.empty())
            return new_node(ParsedRegex::CharacterType, (Codepoint)character_class.ctypes);

        auto class_id = m_parsed_regex.character_classes.size();
        m_parsed_regex.character_classes.push_back(std::move(character_class));

        return new_node(ParsedRegex::Class, class_id);
    }

    ParsedRegex::Quantifier quantifier()
    {
        if (at_end())
            return {ParsedRegex::Quantifier::One};

        constexpr int max_repeat = 1000;
        auto read_bound = [&]() {
            int16_t res = 0;
            for (auto begin = m_pos; m_pos != m_regex.end(); ++m_pos)
            {
                const auto cp = *m_pos;
                if (cp < '0' or cp > '9')
                    return m_pos == begin ? (int16_t)-1 : res;
                res = res * 10 + cp - '0';
                if (res > max_repeat)
                    parse_error(format("Explicit quantifier is too big, maximum is {}", max_repeat));
            }
            return res;
        };

        auto check_greedy = [&]() {
            if (at_end() or *m_pos != '?')
                return true;
            ++m_pos;
            return false;
        };

        switch (*m_pos)
        {
            case '*': ++m_pos; return {ParsedRegex::Quantifier::RepeatZeroOrMore, check_greedy()};
            case '+': ++m_pos; return {ParsedRegex::Quantifier::RepeatOneOrMore, check_greedy()};
            case '?': ++m_pos; return {ParsedRegex::Quantifier::Optional, check_greedy()};
            case '{':
            {
                ++m_pos;
                const int16_t min = read_bound();
                int16_t max = min;
                if (*m_pos == ',')
                {
                    ++m_pos;
                    max = read_bound();
                }
                if (*m_pos++ != '}')
                   parse_error("expected closing bracket");
                return {ParsedRegex::Quantifier::RepeatMinMax, check_greedy(), min, max};
            }
            default: return {ParsedRegex::Quantifier::One};
        }
    }

    NodeIndex new_node(ParsedRegex::Op op, Codepoint value = -1,
                          ParsedRegex::Quantifier quantifier = {ParsedRegex::Quantifier::One})
    {
        constexpr auto max_nodes = std::numeric_limits<int16_t>::max();
        const NodeIndex res = m_parsed_regex.nodes.size();
        if (res == max_nodes)
            parse_error(format("regex parsed to more than {} ast nodes", max_nodes));
        const NodeIndex next = res+1;
        m_parsed_regex.nodes.push_back({op, m_flags & Flags::IgnoreCase, next, value, quantifier});
        return res;
    }

    bool at_end() const { return m_pos == m_regex.end(); }

    ParsedRegex::Node& get_node(NodeIndex index)
    {
        return m_parsed_regex.nodes[index];
    }


    [[gnu::noreturn]]
    void parse_error(StringView error) const
    {
        throw regex_error(format("regex parse error: {} at '{}<<<HERE>>>{}'", error,
                                 StringView{m_regex.begin(), m_pos.base()},
                                 StringView{m_pos.base(), m_regex.end()}));
    }

    void validate_lookaround(NodeIndex index)
    {
        using Lookaround = CompiledRegex::Lookaround;
        for (auto child_index : Children<>{m_parsed_regex, index})
        {
            auto& child = get_node(child_index);
            if (child.op != ParsedRegex::Literal and child.op != ParsedRegex::Class and
                child.op != ParsedRegex::CharacterType and child.op != ParsedRegex::AnyChar and
                child.op != ParsedRegex::AnyCharExceptNewLine)
                parse_error("Lookaround can only contain literals, any chars or character classes");
            if (child.op == ParsedRegex::Literal and
                to_underlying(Lookaround::OpBegin) <= child.value and
                child.value < to_underlying(Lookaround::OpEnd))
                parse_error("Lookaround does not support literals codepoint between 0xF0000 and 0xFFFFD");
            if (child.quantifier.type != ParsedRegex::Quantifier::One)
                parse_error("Quantifiers cannot be used in lookarounds");
        }
    }

    ParsedRegex m_parsed_regex;
    StringView m_regex;
    Iterator m_pos;

    Flags m_flags = Flags::DotMatchesNewLine;

    static constexpr struct CharacterClassEscape {
        Codepoint cp;
        CharacterType ctype;
    } character_class_escapes[] = {
        { 'd', CharacterType::Digit },                { 'D', CharacterType::NotDigit },
        { 'w', CharacterType::Word },                 { 'W', CharacterType::NotWord },
        { 's', CharacterType::Whitespace },           { 'S', CharacterType::NotWhitespace },
        { 'h', CharacterType::HorizontalWhitespace }, { 'H', CharacterType::NotHorizontalWhitespace },
    };

    static constexpr struct ControlEscape {
        Codepoint name;
        Codepoint value;
    } control_escapes[] = {
        { 'f', '\f' },
        { 'n', '\n' },
        { 'r', '\r' },
        { 't', '\t' },
        { 'v', '\v' }
    };
};

constexpr RegexParser::CharacterClassEscape RegexParser::character_class_escapes[];
constexpr RegexParser::ControlEscape RegexParser::control_escapes[];

struct RegexCompiler
{
    RegexCompiler(ParsedRegex&& parsed_regex, RegexCompileFlags flags)
        : m_flags(flags), m_parsed_regex{parsed_regex}
    {
        kak_assert(not (flags & RegexCompileFlags::NoForward) or flags & RegexCompileFlags::Backward);
        // Approximation of the number of instructions generated
        m_program.instructions.reserve((parsed_regex.nodes.size() + 1)
                                       * (((flags & RegexCompileFlags::Backward) and
                                           not (flags & RegexCompileFlags::NoForward)) ? 2 : 1));

        if (not (flags & RegexCompileFlags::NoForward))
        {
            m_program.forward_start_desc = compute_start_desc<RegexMode::Forward>();
            compile_node<RegexMode::Forward>(0);
            push_inst(CompiledRegex::Match);
        }

        if (flags & RegexCompileFlags::Backward)
        {
            m_program.first_backward_inst = m_program.instructions.size();
            m_program.backward_start_desc = compute_start_desc<RegexMode::Backward>();
            compile_node<RegexMode::Backward>(0);
            push_inst(CompiledRegex::Match);
        }
        else
            m_program.first_backward_inst = -1;

        m_program.character_classes = std::move(m_parsed_regex.character_classes);
        m_program.named_captures = std::move(m_parsed_regex.named_captures);
        m_program.save_count = m_parsed_regex.capture_count * 2;
    }

    CompiledRegex get_compiled_regex() { return std::move(m_program); }

private:

    template<RegexMode direction>
    uint32_t compile_node_inner(ParsedRegex::NodeIndex index)
    {
        auto& node = get_node(index);

        const uint32_t start_pos = (uint32_t)m_program.instructions.size();
        const bool ignore_case = node.ignore_case;

        const bool save = (node.op == ParsedRegex::Alternation or node.op == ParsedRegex::Sequence) and
                          (node.value == 0 or (node.value != -1 and not (m_flags & RegexCompileFlags::NoSubs)));
        constexpr bool forward = direction == RegexMode::Forward;
        if (save)
            push_inst(CompiledRegex::Save, node.value * 2 + (forward ? 0 : 1));

        Vector<uint32_t> goto_inner_end_offsets;
        switch (node.op)
        {
            case ParsedRegex::Literal:
                if (ignore_case)
                    push_inst(CompiledRegex::Literal_IgnoreCase, to_lower(node.value));
                else
                    push_inst(CompiledRegex::Literal, node.value);
                break;
            case ParsedRegex::AnyChar:
                push_inst(CompiledRegex::AnyChar);
                break;
            case ParsedRegex::AnyCharExceptNewLine:
                push_inst(CompiledRegex::AnyCharExceptNewLine);
                break;
            case ParsedRegex::Class:
                push_inst(CompiledRegex::Class, node.value);
                break;
            case ParsedRegex::CharacterType:
                push_inst(CompiledRegex::CharacterType, node.value);
                break;
            case ParsedRegex::Sequence:
            {
                for (auto child : Children<direction>{m_parsed_regex, index})
                    compile_node<direction>(child);
                break;
            }
            case ParsedRegex::Alternation:
            {
                auto split_pos = m_program.instructions.size();
                for (auto child : Children<>{m_parsed_regex, index})
                {
                    if (child != index+1)
                        push_inst(CompiledRegex::Split_PrioritizeParent);
                }

                const auto end = node.children_end;
                for (auto child : Children<>{m_parsed_regex, index})
                {
                    auto node = compile_node<direction>(child);
                    if (child != index+1)
                        m_program.instructions[split_pos++].param = node;
                    if (get_node(child).children_end != end)
                    {
                        auto jump = push_inst(CompiledRegex::Jump);
                        goto_inner_end_offsets.push_back(jump);
                    }
                }
                break;
            }
            case ParsedRegex::LookAhead:
                push_inst(ignore_case ? CompiledRegex::LookAhead_IgnoreCase
                                      : CompiledRegex::LookAhead,
                          push_lookaround<RegexMode::Forward>(index, ignore_case));
                break;
            case ParsedRegex::NegativeLookAhead:
                push_inst(ignore_case ? CompiledRegex::NegativeLookAhead_IgnoreCase
                                      : CompiledRegex::NegativeLookAhead,
                          push_lookaround<RegexMode::Forward>(index, ignore_case));
                break;
            case ParsedRegex::LookBehind:
                push_inst(ignore_case ? CompiledRegex::LookBehind_IgnoreCase
                                      : CompiledRegex::LookBehind,
                          push_lookaround<RegexMode::Backward>(index, ignore_case));
                break;
            case ParsedRegex::NegativeLookBehind:
                push_inst(ignore_case ? CompiledRegex::NegativeLookBehind_IgnoreCase
                                      : CompiledRegex::NegativeLookBehind,
                          push_lookaround<RegexMode::Backward>(index, ignore_case));
                break;
            case ParsedRegex::LineStart:
                push_inst(CompiledRegex::LineStart);
                break;
            case ParsedRegex::LineEnd:
                push_inst(CompiledRegex::LineEnd);
                break;
            case ParsedRegex::WordBoundary:
                push_inst(CompiledRegex::WordBoundary);
                break;
            case ParsedRegex::NotWordBoundary:
                push_inst(CompiledRegex::NotWordBoundary);
                break;
            case ParsedRegex::SubjectBegin:
                push_inst(CompiledRegex::SubjectBegin);
                break;
            case ParsedRegex::SubjectEnd:
                push_inst(CompiledRegex::SubjectEnd);
                break;
            case ParsedRegex::ResetStart:
                push_inst(CompiledRegex::Save, 0);
                break;
        }

        for (auto& offset : goto_inner_end_offsets)
            m_program.instructions[offset].param = m_program.instructions.size();

        if (save)
            push_inst(CompiledRegex::Save, node.value * 2 + (forward ? 1 : 0));

        return start_pos;
    }

    template<RegexMode direction>
    uint32_t compile_node(ParsedRegex::NodeIndex index)
    {
        auto& node = get_node(index);

        const uint32_t start_pos = (uint32_t)m_program.instructions.size();
        Vector<uint32_t> goto_ends;

        auto& quantifier = node.quantifier;

        if (quantifier.allows_none())
        {
            auto split_pos = push_inst(quantifier.greedy ? CompiledRegex::Split_PrioritizeParent
                                                         : CompiledRegex::Split_PrioritizeChild);
            goto_ends.push_back(split_pos);
        }

        auto inner_pos = compile_node_inner<direction>(index);
        // Write the node multiple times when we have a min count quantifier
        for (int i = 1; i < quantifier.min; ++i)
            inner_pos = compile_node_inner<direction>(index);

        if (quantifier.allows_infinite_repeat())
            push_inst(quantifier.greedy ? CompiledRegex::Split_PrioritizeChild
                                        : CompiledRegex::Split_PrioritizeParent,
                      inner_pos);
        // Write the node as an optional match for the min -> max counts
        else for (int i = std::max((int16_t)1, quantifier.min); // STILL UGLY !
                  i < quantifier.max; ++i)
        {
            auto split_pos = push_inst(quantifier.greedy ? CompiledRegex::Split_PrioritizeParent
                                                         : CompiledRegex::Split_PrioritizeChild);
            goto_ends.push_back(split_pos);
            compile_node_inner<direction>(index);
        }

        for (auto offset : goto_ends)
            m_program.instructions[offset].param = m_program.instructions.size();

        return start_pos;
    }

    uint32_t push_inst(CompiledRegex::Op op, uint32_t param = 0)
    {
        constexpr auto max_instructions = std::numeric_limits<int16_t>::max();
        const uint32_t res = m_program.instructions.size();
        if (res > max_instructions)
            throw regex_error(format("regex compiled to more than {} instructions", max_instructions));
        m_program.instructions.push_back({ op, false, 0, param });
        return res;
    }

    template<RegexMode direction>
    uint32_t push_lookaround(ParsedRegex::NodeIndex index, bool ignore_case)
    {
        using Lookaround = CompiledRegex::Lookaround;

        const uint32_t res = m_program.lookarounds.size();
        for (auto child : Children<direction>{m_parsed_regex, index})
        {
            auto& character = get_node(child);
            if (character.op == ParsedRegex::Literal)
                m_program.lookarounds.push_back(
                    static_cast<Lookaround>(ignore_case ? to_lower(character.value) : character.value));
            else if (character.op == ParsedRegex::AnyChar)
                m_program.lookarounds.push_back(Lookaround::AnyChar);
            else if (character.op == ParsedRegex::AnyCharExceptNewLine)
                m_program.lookarounds.push_back(Lookaround::AnyCharExceptNewLine);
            else if (character.op == ParsedRegex::Class)
                m_program.lookarounds.push_back(static_cast<Lookaround>(to_underlying(Lookaround::CharacterClass) + character.value));
            else if (character.op == ParsedRegex::CharacterType)
                m_program.lookarounds.push_back(static_cast<Lookaround>(to_underlying(Lookaround::CharacterType) | character.value));
            else
                kak_assert(false);
        }
        m_program.lookarounds.push_back(Lookaround::EndOfLookaround);
        return res;
    }

    // Mutate start_desc with informations on which Codepoint could start a match.
    // Returns true if the node possibly does not consume the char, in which case
    // the next node would still be relevant for the parent node start chars computation.
    template<RegexMode direction>
    bool compute_start_desc(ParsedRegex::NodeIndex index,
                             CompiledRegex::StartDesc& start_desc) const
    {
        auto& node = get_node(index);
        switch (node.op)
        {
            case ParsedRegex::Literal:
                if (node.value < CompiledRegex::StartDesc::count)
                {
                    if (node.ignore_case)
                    {
                        start_desc.map[to_lower(node.value)] = true;
                        start_desc.map[to_upper(node.value)] = true;
                    }
                    else
                        start_desc.map[node.value] = true;
                }
                else
                    start_desc.map[CompiledRegex::StartDesc::other] = true;
                return node.quantifier.allows_none();
            case ParsedRegex::AnyChar:
                for (auto& b : start_desc.map)
                    b = true;
               return node.quantifier.allows_none();
            case ParsedRegex::AnyCharExceptNewLine:
                for (Codepoint cp = 0; cp < CompiledRegex::StartDesc::count; ++cp)
                {
                    if (cp != '\n')
                        start_desc.map[cp] = true;
                }
               return node.quantifier.allows_none();
            case ParsedRegex::Class:
            {
                auto& character_class = m_parsed_regex.character_classes[node.value];
                if (character_class.ctypes == CharacterType::None and not character_class.negative)
                {
                    for (auto& range : character_class.ranges)
                    {
                        const auto clamp = [](Codepoint cp) { return std::min(CompiledRegex::StartDesc::count, cp); };
                        for (auto cp = clamp(range.min), end = clamp(range.max + 1); cp < end; ++cp)
                            start_desc.map[cp] = true;
                        if (range.max >= CompiledRegex::StartDesc::count)
                            start_desc.map[CompiledRegex::StartDesc::other] = true;
                    }
                }
                else
                {
                    for (Codepoint cp = 0; cp < CompiledRegex::StartDesc::count; ++cp)
                    {
                        if (start_desc.map[cp] or is_character_class(character_class, cp))
                            start_desc.map[cp] = true;
                    }
                }
                start_desc.map[CompiledRegex::StartDesc::other] = true;
                return node.quantifier.allows_none();
            }
            case ParsedRegex::CharacterType:
            {
                const CharacterType ctype = (CharacterType)node.value;
                for (Codepoint cp = 0; cp < CompiledRegex::StartDesc::count; ++cp)
                {
                    if (is_ctype(ctype, cp))
                        start_desc.map[cp] = true;
                }
                start_desc.map[CompiledRegex::StartDesc::other] = true;
                return node.quantifier.allows_none();
            }
            case ParsedRegex::Sequence:
            {
                for (auto child : Children<direction>{m_parsed_regex, index})
                {
                    if (not compute_start_desc<direction>(child, start_desc))
                        return node.quantifier.allows_none();
                }
                return true;
            }
            case ParsedRegex::Alternation:
            {
                bool all_consumed = not node.quantifier.allows_none();
                for (auto child : Children<>{m_parsed_regex, index})
                {
                    if (compute_start_desc<direction>(child, start_desc))
                        all_consumed = false;
                }
                return not all_consumed;
            }
            case ParsedRegex::LineStart:
            case ParsedRegex::LineEnd:
            case ParsedRegex::WordBoundary:
            case ParsedRegex::NotWordBoundary:
            case ParsedRegex::SubjectBegin:
            case ParsedRegex::SubjectEnd:
            case ParsedRegex::ResetStart:
            case ParsedRegex::LookAhead:
            case ParsedRegex::LookBehind:
            case ParsedRegex::NegativeLookAhead:
            case ParsedRegex::NegativeLookBehind:
                return true;
        }
        return false;
    }

    template<RegexMode direction>
    [[gnu::noinline]]
    std::unique_ptr<CompiledRegex::StartDesc> compute_start_desc() const
    {
        CompiledRegex::StartDesc start_desc{};
        if (compute_start_desc<direction>(0, start_desc) or
            not contains(start_desc.map, false))
            return nullptr;

        return std::make_unique<CompiledRegex::StartDesc>(start_desc);
    }

    const ParsedRegex::Node& get_node(ParsedRegex::NodeIndex index) const
    {
        return m_parsed_regex.nodes[index];
    }

    CompiledRegex m_program;
    RegexCompileFlags m_flags;
    ParsedRegex& m_parsed_regex;
};

String dump_regex(const CompiledRegex& program)
{
    String res;
    int count = 0;
    for (auto& inst : program.instructions)
    {
        char buf[20];
        sprintf(buf, " %03d     ", count++);
        res += buf;
        switch (inst.op)
        {
            case CompiledRegex::Literal:
                res += format("literal {}\n", inst.param);
                break;
            case CompiledRegex::Literal_IgnoreCase:
                res += format("literal (ignore case) {}\n", inst.param);
                break;
            case CompiledRegex::AnyChar:
                res += "any char\n";
                break;
            case CompiledRegex::AnyCharExceptNewLine:
                res += "anything but newline\n";
                break;
            case CompiledRegex::Jump:
                res += format("jump {}\n", inst.param);
                break;
            case CompiledRegex::Split_PrioritizeParent:
            case CompiledRegex::Split_PrioritizeChild:
            {
                res += format("split (prioritize {}) {}\n",
                              inst.op == CompiledRegex::Split_PrioritizeParent ? "parent" : "child",
                              inst.param);
                break;
            }
            case CompiledRegex::Save:
                res += format("save {}\n", inst.param);
                break;
            case CompiledRegex::Class:
                res += format("class {}\n", inst.param);
                break;
            case CompiledRegex::CharacterType:
                res += format("character type {}\n", inst.param);
                break;
            case CompiledRegex::LineStart:
                res += "line start\n";
                break;
            case CompiledRegex::LineEnd:
                res += "line end\n";
                break;
            case CompiledRegex::WordBoundary:
                res += "word boundary\n";
                break;
            case CompiledRegex::NotWordBoundary:
                res += "not word boundary\n";
                break;
            case CompiledRegex::SubjectBegin:
                res += "subject begin\n";
                break;
            case CompiledRegex::SubjectEnd:
                res += "subject end\n";
                break;
            case CompiledRegex::LookAhead:
            case CompiledRegex::NegativeLookAhead:
            case CompiledRegex::LookBehind:
            case CompiledRegex::NegativeLookBehind:
            case CompiledRegex::LookAhead_IgnoreCase:
            case CompiledRegex::NegativeLookAhead_IgnoreCase:
            case CompiledRegex::LookBehind_IgnoreCase:
            case CompiledRegex::NegativeLookBehind_IgnoreCase:
            {
                const char* name = nullptr;
                if (inst.op == CompiledRegex::LookAhead)
                    name = "look ahead";
                if (inst.op == CompiledRegex::NegativeLookAhead)
                    name = "negative look ahead";
                if (inst.op == CompiledRegex::LookBehind)
                    name = "look behind";
                if (inst.op == CompiledRegex::NegativeLookBehind)
                    name = "negative look behind";

                if (inst.op == CompiledRegex::LookAhead_IgnoreCase)
                    name = "look ahead (ignore case)";
                if (inst.op == CompiledRegex::NegativeLookAhead_IgnoreCase)
                    name = "negative look ahead (ignore case)";
                if (inst.op == CompiledRegex::LookBehind_IgnoreCase)
                    name = "look behind (ignore case)";
                if (inst.op == CompiledRegex::NegativeLookBehind_IgnoreCase)
                    name = "negative look behind (ignore case)";

                String str;
                for (auto it = program.lookarounds.begin() + inst.param;
                     *it != CompiledRegex::Lookaround::EndOfLookaround; ++it)
                    utf8::dump(std::back_inserter(str), to_underlying(*it));
                res += format("{} ({})\n", name, str);
                break;
            }
            case CompiledRegex::Match:
                res += "match\n";
        }
    }
    auto dump_start_desc = [&](CompiledRegex::StartDesc& desc, StringView name) {
        res += name + " start desc: [";
        for (size_t c = 0; c < CompiledRegex::StartDesc::count; ++c)
        {
            if (desc.map[c])
            {
                if (c < 32)
                    res += format("<0x{}>", Hex{c});
                else
                    res += (char)c;
            }
        }
        res += "]\n";
    };
    if (program.forward_start_desc)
        dump_start_desc(*program.forward_start_desc, "forward");
    if (program.backward_start_desc)
        dump_start_desc(*program.backward_start_desc, "backward");
    return res;
}

CompiledRegex compile_regex(StringView re, RegexCompileFlags flags)
{
    return RegexCompiler{RegexParser::parse(re), flags}.get_compiled_regex();
}

bool is_character_class(const CharacterClass& character_class, Codepoint cp)
{
    if (character_class.ignore_case)
        cp = to_lower(cp);

    auto it = std::lower_bound(character_class.ranges.begin(),
                               character_class.ranges.end(), cp,
                               [](auto& range, Codepoint cp)
                               { return range.max < cp; });

    auto found = (it != character_class.ranges.end() and it->min <= cp) or
                 is_ctype(character_class.ctypes, cp);

    return found != character_class.negative;
}

bool is_ctype(CharacterType ctype, Codepoint cp)
{
    return ((ctype & CharacterType::Whitespace)              and     is_blank(cp))            or
           ((ctype & CharacterType::HorizontalWhitespace)    and     is_horizontal_blank(cp)) or
           ((ctype & CharacterType::Digit)                   and     iswdigit(cp))            or
           ((ctype & CharacterType::Word)                    and     is_word(cp))             or
           ((ctype & CharacterType::NotWhitespace)           and not is_blank(cp))            or
           ((ctype & CharacterType::NotHorizontalWhitespace) and not is_horizontal_blank(cp)) or
           ((ctype & CharacterType::NotDigit)                and not iswdigit(cp))            or
           ((ctype & CharacterType::NotWord)                 and not is_word(cp));
}

namespace
{
template<RegexMode mode = RegexMode::Forward>
struct TestVM : CompiledRegex, ThreadedRegexVM<const char*, mode>
{
    using VMType = ThreadedRegexVM<const char*, mode>;

    TestVM(StringView re, bool dump = false)
        : CompiledRegex{compile_regex(re, mode & RegexMode::Forward ?
                                          RegexCompileFlags::None : RegexCompileFlags::Backward)},
          VMType{(const CompiledRegex&)*this}
    { if (dump) puts(dump_regex(*this).c_str()); }

    bool exec(StringView re, RegexExecFlags flags = RegexExecFlags::None)
    {
        return VMType::exec(re.begin(), re.end(), re.begin(), re.end(), flags);
    }
};
}

auto test_regex = UnitTest{[]{
    {
        TestVM<> vm{R"(a*b)"};
        kak_assert(vm.exec("b"));
        kak_assert(vm.exec("ab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("acb"));
        kak_assert(not vm.exec("abc"));
        kak_assert(not vm.exec(""));
    }

    {
        TestVM<> vm{R"(^a.*b$)"};
        kak_assert(vm.exec("afoob"));
        kak_assert(vm.exec("ab"));
        kak_assert(not vm.exec("bab"));
        kak_assert(not vm.exec(""));
    }

    {
        TestVM<> vm{R"(^(foo|qux|baz)+(bar)?baz$)"};
        kak_assert(vm.exec("fooquxbarbaz"));
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == "qux");
        kak_assert(not vm.exec("fooquxbarbaze"));
        kak_assert(not vm.exec("quxbar"));
        kak_assert(not vm.exec("blahblah"));
        kak_assert(vm.exec("bazbaz"));
        kak_assert(vm.exec("quxbaz"));
    }

    {
        TestVM<> vm{R"(.*\b(foo|bar)\b.*)"};
        kak_assert(vm.exec("qux foo baz"));
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == "foo");
        kak_assert(not vm.exec("quxfoobaz"));
        kak_assert(vm.exec("bar"));
        kak_assert(not vm.exec("foobar"));
    }

    {
        TestVM<> vm{R"((foo|bar))"};
        kak_assert(vm.exec("foo"));
        kak_assert(vm.exec("bar"));
        kak_assert(not vm.exec("foobar"));
    }

    {
        TestVM<> vm{R"(a{3,5}b)"};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaaaab"));
        kak_assert(vm.exec("aaaaab"));
    }

    {
        TestVM<> vm{R"(a{3}b)"};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaab"));
    }

    {
        TestVM<> vm{R"(a{3,}b)"};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(vm.exec("aaaaab"));
    }

    {
        TestVM<> vm{R"(a{,3}b)"};
        kak_assert(vm.exec("b"));
        kak_assert(vm.exec("ab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaab"));
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"(f.*a(.*o))"};
        kak_assert(vm.exec("blahfoobarfoobaz"));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]} == "foobarfoo");
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == "rfoo");
        kak_assert(vm.exec("mais que fais la police"));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]} == "fais la po");
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == " po");
    }

    {
        TestVM<> vm{R"([àb-dX-Z-]{3,5})"};
        kak_assert(vm.exec("cà-Y"));
        kak_assert(not vm.exec("àeY"));
        kak_assert(vm.exec("dcbàX"));
        kak_assert(not vm.exec("efg"));
    }

    {
        TestVM<> vm{R"((a{3,5})a+)"};
        kak_assert(vm.exec("aaaaaa", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == "aaaaa");
    }

    {
        TestVM<> vm{R"((a{3,5}?)a+)"};
        kak_assert(vm.exec("aaaaaa", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == "aaa");
    }

    {
        TestVM<> vm{R"((a{3,5}?)a)"};
        kak_assert(vm.exec("aaaa"));
    }

    {
        TestVM<> vm{R"(\d{3})"};
        kak_assert(vm.exec("123"));
        kak_assert(not vm.exec("1x3"));
    }

    {
        TestVM<> vm{R"([-\d]+)"};
        kak_assert(vm.exec("123-456"));
        kak_assert(not vm.exec("123_456"));
    }

    {
        TestVM<> vm{R"([ \H]+)"};
        kak_assert(vm.exec("abc "));
        kak_assert(not vm.exec("a \t"));
    }

    {
        TestVM<> vm{R"(\Q{}[]*+?\Ea+)"};
        kak_assert(vm.exec("{}[]*+?aa"));
    }

    {
        TestVM<> vm{R"(\Q...)"};
        kak_assert(vm.exec("..."));
        kak_assert(not vm.exec("bla"));
    }

    {
        TestVM<RegexMode::Forward> vm{R"(foo\Kbar)"};
        kak_assert(vm.exec("foobar"));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]} == "bar");
        kak_assert(not vm.exec("bar"));
    }

    {
        TestVM<RegexMode::Forward> vm{R"((fo+?).*)"};
        kak_assert(vm.exec("foooo"));
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == "fo");
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"((?=fo[\w]).)"};
        kak_assert(vm.exec("barfoo"));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]} == "f");
    }

    {
        TestVM<> vm{R"((?<!f).)"};
        kak_assert(vm.exec("f"));
    }

    {
        TestVM<> vm{R"((?!f[oa]o)...)"};
        kak_assert(not vm.exec("foo"));
        kak_assert(vm.exec("qux"));
    }

    {
        TestVM<> vm{R"(...(?<=f\w.))"};
        kak_assert(vm.exec("foo"));
        kak_assert(not vm.exec("qux"));
    }

    {
        TestVM<> vm{R"(...(?<!foo))"};
        kak_assert(not vm.exec("foo"));
        kak_assert(vm.exec("qux"));
    }

    {
        TestVM<> vm{R"(Foo(?i)f[oB]+)"};
        kak_assert(vm.exec("FooFOoBb"));
    }

    {
        TestVM<> vm{R"([^\]]+)"};
        kak_assert(not vm.exec("a]c"));
        kak_assert(vm.exec("abc"));
    }

    {
        TestVM<> vm{R"([^:\n]+)"};
        kak_assert(not vm.exec("\nbc"));
        kak_assert(vm.exec("abc"));
    }

    {
        TestVM<> vm{R"((?:foo)+)"};
        kak_assert(vm.exec("foofoofoo"));
        kak_assert(not vm.exec("barbarbar"));
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"((?<!\\)(?:\\\\)*")"};
        kak_assert(vm.exec("foo\""));
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"($)"};
        kak_assert(vm.exec("foo\n"));
        kak_assert(*vm.captures()[0] == '\n');
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"(fo{1,})"};
        kak_assert(vm.exec("foo1fooo2"));
        kak_assert(*vm.captures()[1] == '2');
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"((?<=f)oo(b[ae]r)?(?=baz))"};
        kak_assert(vm.exec("foobarbazfoobazfooberbaz"));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]}  == "oober");
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]}  == "ber");
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"((baz|boz|foo|qux)(?<!baz)(?<!o))"};
        kak_assert(vm.exec("quxbozfoobaz"));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]}  == "boz");
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"(foo)"};
        kak_assert(vm.exec("foofoo"));
        kak_assert(*vm.captures()[1]  == 0);
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"($)"};
        kak_assert(vm.exec("foo\nbar\nbaz\nqux", RegexExecFlags::NotEndOfLine));
        kak_assert(StringView{vm.captures()[0]}  == "\nqux");
        kak_assert(vm.exec("foo\nbar\nbaz\nqux", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[0]}  == "");
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"(^)"};
        kak_assert(not vm.exec("foo", RegexExecFlags::NotBeginOfLine));
        kak_assert(vm.exec("foo", RegexExecFlags::None));
        kak_assert(vm.exec("foo\nbar", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[0]}  == "bar");
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"(\A\w+)"};
        kak_assert(vm.exec("foo\nbar\nbaz", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]}  == "foo");
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"(\b\w+\z)"};
        kak_assert(vm.exec("foo\nbar\nbaz", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]}  == "baz");
    }

    {
        TestVM<RegexMode::Backward | RegexMode::Search> vm{R"(a[^\n]*\n|\n)"};
        kak_assert(vm.exec("foo\nbar\nb", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[0], vm.captures()[1]}  == "ar\n");
    }

    {
        TestVM<> vm{R"(()*)"};
        kak_assert(not vm.exec(" "));
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"(\b(?<!-)(a|b|)(?!-)\b)"};
        kak_assert(vm.exec("# foo bar", RegexExecFlags::None));
        kak_assert(*vm.captures()[0] == '#');
    }

    {
        TestVM<> vm{R"((?=))"};
        kak_assert(vm.exec(""));
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"((?i)FOO)"};
        kak_assert(vm.exec("foo", RegexExecFlags::None));
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"(.?(?=foo))"};
        kak_assert(vm.exec("afoo", RegexExecFlags::None));
        kak_assert(*vm.captures()[0] == 'a');
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"((?i)(?=Foo))"};
        kak_assert(vm.exec("fOO", RegexExecFlags::None));
        kak_assert(*vm.captures()[0] == 'f');
    }

    {
        TestVM<> vm{R"([d-ea-dcf-k]+)"};
        kak_assert(vm.exec("abcde"));
    }

    {
        TestVM<> vm{R"((?i)[a-c]+)"};
        kak_assert(vm.exec("bCa"));
    }

    {
        TestVM<RegexMode::Forward | RegexMode::Search> vm{R"(д)"};
        kak_assert(vm.exec("д", RegexExecFlags::None));
    }

    {
        TestVM<> vm{R"(\0\x0A\u260e\u260F)"};
        const char str[] = "\0\n☎☏"; // work around the null byte in the literal
        kak_assert(vm.exec({str, str + sizeof(str)-1}));
    }

    {
        auto eq = [](const CompiledRegex::NamedCapture& lhs,
                     const CompiledRegex::NamedCapture& rhs) {
            return lhs.name == rhs.name and
                   lhs.index == rhs.index;
        };

        TestVM<> vm{R"((?<year>\d+)-(?<month>\d+)-(?<day>\d+))"};
        kak_assert(vm.exec("2019-01-03", RegexExecFlags::None));
        kak_assert(StringView{vm.captures()[2], vm.captures()[3]} == "2019");
        kak_assert(StringView{vm.captures()[4], vm.captures()[5]} == "01");
        kak_assert(StringView{vm.captures()[6], vm.captures()[7]} == "03");
        kak_assert(vm.named_captures.size() == 3);
        kak_assert(eq(vm.named_captures[0], {"year", 1}));
        kak_assert(eq(vm.named_captures[1], {"month", 2}));
        kak_assert(eq(vm.named_captures[2], {"day", 3}));
    }
}};

}
