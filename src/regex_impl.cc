#include "regex_impl.hh"
#include "vector.hh"
#include "unit_tests.hh"
#include "string.hh"
#include "unicode.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "exception.hh"
#include "array_view.hh"

#include "buffer_utils.hh"

namespace Kakoune
{

struct ParsedRegex
{
    enum Op
    {
        Literal,
        AnyChar,
        Matcher,
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
        LookBehind,
        NegativeLookAhead,
        NegativeLookBehind,
    };

    struct Quantifier
    {
        enum Type
        {
            One,
            Optional,
            RepeatZeroOrMore,
            RepeatOneOrMore,
            RepeatMinMax,
        };
        Type type = One;
        bool greedy = true;
        int min = -1, max = -1;

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
                  (type == Quantifier::RepeatMinMax and max == -1);
        };
    };

    struct AstNode
    {
        Op op;
        Codepoint value;
        Quantifier quantifier;
        bool ignore_case;
        Vector<std::unique_ptr<AstNode>> children;
    };

    using AstNodePtr = std::unique_ptr<AstNode>;

    AstNodePtr ast;
    size_t capture_count;
    Vector<std::function<bool (Codepoint)>> matchers;
};

// Recursive descent parser based on naming used in the ECMAScript
// standard, although the syntax is not fully compatible.
struct RegexParser
{
    RegexParser(StringView re)
        : m_regex{re}, m_pos{re.begin(), re}
    {
        m_parsed_regex.capture_count = 1;
        m_parsed_regex.ast = disjunction(0);
    }

    ParsedRegex get_parsed_regex() { return std::move(m_parsed_regex); }

    static ParsedRegex parse(StringView re) { return RegexParser{re}.get_parsed_regex(); }

private:
    struct InvalidPolicy
    {
        Codepoint operator()(Codepoint cp) { throw runtime_error{"Invalid utf8 in regex"}; }
    };

    using Iterator = utf8::iterator<const char*, Codepoint, int, InvalidPolicy>;
    using AstNodePtr = ParsedRegex::AstNodePtr;

    AstNodePtr disjunction(unsigned capture = -1)
    {
        AstNodePtr node = alternative();
        if (at_end() or *m_pos != '|')
        {
            node->value = capture;
            return node;
        }

        ++m_pos;
        AstNodePtr res = new_node(ParsedRegex::Alternation);
        res->children.push_back(std::move(node));
        res->children.push_back(disjunction());
        res->value = capture;
        return res;
    }

    AstNodePtr alternative(ParsedRegex::Op op = ParsedRegex::Sequence)
    {
        AstNodePtr res = new_node(op);
        while (auto node = term())
            res->children.push_back(std::move(node));
        return res;
    }

    AstNodePtr term()
    {
        if (auto node = assertion())
            return node;
        if (auto node = atom())
        {
            node->quantifier = quantifier();
            return node;
        }
        return nullptr;
    }

    AstNodePtr assertion()
    {
        if (at_end())
            return nullptr;

        switch (*m_pos)
        {
            case '^': ++m_pos; return new_node(ParsedRegex::LineStart);
            case '$': ++m_pos; return new_node(ParsedRegex::LineEnd);
            case '\\':
                if (m_pos+1 == m_regex.end())
                    return nullptr;
                switch (*(m_pos+1))
                {
                    case 'b': m_pos += 2; return new_node(ParsedRegex::WordBoundary);
                    case 'B': m_pos += 2; return new_node(ParsedRegex::NotWordBoundary);
                    case 'A': m_pos += 2; return new_node(ParsedRegex::SubjectBegin);
                    case 'z': m_pos += 2; return new_node(ParsedRegex::SubjectEnd);
                    case 'K': m_pos += 2; return new_node(ParsedRegex::ResetStart);
                }
                break;
            /* TODO: look ahead, look behind */
        }
        return nullptr;
    }

    AstNodePtr atom()
    {
        if (at_end())
            return nullptr;

        const Codepoint cp = *m_pos;
        switch (cp)
        {
            case '.': ++m_pos; return new_node(ParsedRegex::AnyChar);
            case '(':
            {
                auto advance = [&]() {
                    if (++m_pos == m_regex.end())
                        parse_error("unclosed parenthesis");
                    return *m_pos;
                };

                AstNodePtr content;
                if (advance() == '?')
                {
                    auto c = advance();
                    if (c == ':')
                        content = disjunction(-1);
                    else if (contains("=!<", c))
                    {
                        bool behind = false;
                        if (c == '<')
                        {
                            advance();
                            behind = true;
                        }

                        auto type = *m_pos++;
                        if (type == '=')
                            content = alternative(behind ? ParsedRegex::LookBehind
                                                         : ParsedRegex::LookAhead);
                        else if (type == '!')
                            content = alternative(behind ? ParsedRegex::NegativeLookBehind
                                                         : ParsedRegex::NegativeLookAhead);
                        else
                            parse_error("invalid disjunction");

                         validate_lookaround(content);
                    }
                    else if (c == 'i' or c == 'I')
                    {
                        m_ignore_case = c == 'i';
                        if (advance() != ')')
                            parse_error("unclosed parenthesis");
                        ++m_pos;
                        return atom(); // get next atom
                    }
                    else
                        parse_error("invalid disjunction");
                }
                else
                    content = disjunction(m_parsed_regex.capture_count++);

                if (at_end() or *m_pos != ')')
                    parse_error("unclosed parenthesis");
                ++m_pos;
                return content;
            }
            case '\\':
                ++m_pos;
                return atom_escape();
            case '[':
                ++m_pos;
                return character_class();
            case '|': case ')':
                return nullptr;
            default:
                if (contains("^$.*+?[]{}", cp))
                    parse_error(format("unexpected '{}'", cp));
                ++m_pos;
                return new_node(ParsedRegex::Literal, cp);
        }
    }

    AstNodePtr atom_escape()
    {
        const Codepoint cp = *m_pos++;

        if (cp == 'Q')
        {
            auto escaped_sequence = new_node(ParsedRegex::Sequence);
            constexpr StringView end_mark{"\\E"};
            auto quote_end = std::search(m_pos.base(), m_regex.end(), end_mark.begin(), end_mark.end());
            while (m_pos != quote_end)
                escaped_sequence->children.push_back(new_node(ParsedRegex::Literal, *m_pos++));
            if (quote_end != m_regex.end())
                m_pos += 2;

            return escaped_sequence;
        }

        // CharacterClassEscape
        for (auto& character_class : character_class_escapes)
        {
            if (character_class.cp == cp)
            {
                auto matcher_id = m_parsed_regex.matchers.size();
                m_parsed_regex.matchers.push_back(
                    [ctype = character_class.ctype ? wctype(character_class.ctype) : (wctype_t)0,
                     chars = character_class.additional_chars] (Codepoint cp) {
                        return (ctype != 0 and iswctype(cp, ctype)) or contains(chars, cp);
                    });
                return new_node(ParsedRegex::Matcher, matcher_id);
            }
        }

        // CharacterEscape
        struct { Codepoint name; Codepoint value; } control_escapes[] = {
            { 'f', '\f' }, { 'n', '\n' }, { 'r', '\r' }, { 't', '\t' }, { 'v', '\v' }
        };
        for (auto& control : control_escapes)
        {
            if (control.name == cp)
                return new_node(ParsedRegex::Literal, control.value);
        }

        // TOOD: \c..., \0..., '\0x...', \u...

        if (contains("^$\\.*+?()[]{}|", cp)) // SyntaxCharacter
            return new_node(ParsedRegex::Literal, cp);
        parse_error(format("unknown atom escape '{}'", cp));
    }

    AstNodePtr character_class()
    {
        const bool negative = m_pos != m_regex.end() and *m_pos == '^';
        if (negative)
            ++m_pos;

        struct CharRange { Codepoint min, max; };
        Vector<CharRange> ranges;
        Vector<Codepoint> excluded;
        Vector<std::pair<wctype_t, bool>> ctypes;
        while (m_pos != m_regex.end() and *m_pos != ']')
        {
            auto cp = *m_pos++;
            if (cp == '-')
            {
                ranges.push_back({ '-', '-' });
                continue;
            }

            if (at_end())
                break;

            if (cp == '\\')
            {
                auto it = find_if(character_class_escapes,
                                  [cp = *m_pos](auto& t) { return t.cp == cp; });
                if (it != std::end(character_class_escapes))
                {
                    if (it->ctype)
                        ctypes.push_back({wctype(it->ctype), not it->neg});
                    for (auto& c : it->additional_chars) // TODO: handle negative case
                    {
                        if (it->neg)
                            excluded.push_back((Codepoint)c);
                        else
                            ranges.push_back({(Codepoint)c, (Codepoint)c});
                    }
                    ++m_pos;
                    continue;
                }
                else // its just an escaped character
                {
                    
                    if (++m_pos == m_regex.end())
                        break;
                    cp = *m_pos;
                }
            }

            CharRange range = { cp, cp };
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
                    ranges.push_back(range);
                    range = { '-', '-' };
                }
            }
            ranges.push_back(range);
        }
        if (at_end())
            parse_error("unclosed character class");
        ++m_pos;

        if (m_ignore_case)
        {
            for (auto& range : ranges)
            {
                range.min = to_lower(range.max);
                range.max = to_lower(range.max);
            }
            for (auto& cp : excluded)
                cp = to_lower(cp);
        }

        auto matcher = [ranges = std::move(ranges),
                        ctypes = std::move(ctypes),
                        excluded = std::move(excluded),
                        negative, ignore_case = m_ignore_case] (Codepoint cp) {
            if (ignore_case)
                cp = to_lower(cp);

            auto found = contains_that(ranges, [cp](auto& r) {
                return r.min <= cp and cp <= r.max;
            }) or contains_that(ctypes, [cp](auto& c) {
                return (bool)iswctype(cp, c.first) == c.second;
            }) or (not excluded.empty() and not contains(excluded, cp));
            return negative ? not found : found;
        };

        auto matcher_id = m_parsed_regex.matchers.size();
        m_parsed_regex.matchers.push_back(std::move(matcher));

        return new_node(ParsedRegex::Matcher, matcher_id);
    }

    ParsedRegex::Quantifier quantifier()
    {
        if (at_end())
            return {ParsedRegex::Quantifier::One};

        auto read_int = [](auto& pos, auto begin, auto end) {
            int res = 0;
            for (; pos != end; ++pos)
            {
                const auto cp = *pos;
                if (cp < '0' or cp > '9')
                    return pos == begin ? -1 : res;
                res = res * 10 + cp - '0';
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
                auto it = m_pos+1;
                const int min = read_int(it, it, m_regex.end());
                int max = min;
                if (*it == ',')
                {
                    ++it;
                    max = read_int(it, it, m_regex.end());
                }
                if (*it++ != '}')
                   parse_error("expected closing bracket");
                m_pos = it;
                return {ParsedRegex::Quantifier::RepeatMinMax, check_greedy(), min, max};
            }
            default: return {ParsedRegex::Quantifier::One};
        }
    }

    AstNodePtr new_node(ParsedRegex::Op op, Codepoint value = -1,
                        ParsedRegex::Quantifier quantifier = {ParsedRegex::Quantifier::One})
    {
        return AstNodePtr{new ParsedRegex::AstNode{op, value, quantifier, m_ignore_case, {}}};
    }

    bool at_end() const { return m_pos == m_regex.end(); }

    [[gnu::noreturn]]
    void parse_error(StringView error)
    {
        throw runtime_error(format("regex parse error: {} at '{}<<<HERE>>>{}'", error,
                                   StringView{m_regex.begin(), m_pos.base()},
                                   StringView{m_pos.base(), m_regex.end()}));
    }

    void validate_lookaround(const AstNodePtr& node)
    {
        for (auto& child : node->children)
            if (child->op != ParsedRegex::Literal)
                parse_error("Lookaround can only contain literals");
    }

    ParsedRegex m_parsed_regex;
    StringView m_regex;
    Iterator m_pos;
    bool m_ignore_case = false;

    struct CharacterClassEscape {
        Codepoint cp;
        const char* ctype;
        StringView additional_chars;
        bool neg;
    };

    static const CharacterClassEscape character_class_escapes[8];
};

// For some reason Gcc fails to link if this is constexpr
const RegexParser::CharacterClassEscape RegexParser::character_class_escapes[8] = {
    { 'd', "digit", "", false },
    { 'D', "digit", "", true },
    { 'w', "alnum", "_", false },
    { 'W', "alnum", "_", true },
    { 's', "space", "", false },
    { 'S', "space", "", true },
    { 'h', nullptr, " \t", false },
    { 'H', nullptr, " \t", true },
};

struct RegexCompiler
{
    RegexCompiler(const ParsedRegex& parsed_regex)
        : m_parsed_regex{parsed_regex}
    {
        write_search_prefix();
        compile_node(m_parsed_regex.ast);
        push_op(CompiledRegex::Match);
        m_program.matchers = m_parsed_regex.matchers;
        m_program.save_count = m_parsed_regex.capture_count * 2;
    }

    CompiledRegex get_compiled_regex() { return std::move(m_program); }

    using Offset = CompiledRegex::Offset;

    static CompiledRegex compile(StringView re)
    {
        return RegexCompiler{RegexParser::parse(re)}.get_compiled_regex();
    }

private:
    Offset compile_node_inner(const ParsedRegex::AstNodePtr& node)
    {
        const auto start_pos = m_program.bytecode.size();

        const Codepoint capture = (node->op == ParsedRegex::Alternation or node->op == ParsedRegex::Sequence) ? node->value : -1;
        if (capture != -1)
        {
            push_op(CompiledRegex::Save);
            push_byte(capture * 2);
        }

        Vector<Offset> goto_inner_end_offsets;
        switch (node->op)
        {
            case ParsedRegex::Literal:
                push_op(node->ignore_case ? CompiledRegex::LiteralIgnoreCase
                                          : CompiledRegex::Literal);
                push_codepoint(node->ignore_case ? to_lower(node->value)
                                                 : node->value);
                break;
            case ParsedRegex::AnyChar:
                push_op(CompiledRegex::AnyChar);
                break;
            case ParsedRegex::Matcher:
                push_op(CompiledRegex::Matcher);
                push_byte(node->value);
            case ParsedRegex::Sequence:
                for (auto& child : node->children)
                    compile_node(child);
                break;
            case ParsedRegex::Alternation:
            {
                auto& children = node->children;
                kak_assert(children.size() == 2);

                push_op(CompiledRegex::Split_PrioritizeParent);
                auto offset = alloc_offset();

                compile_node(children[0]);
                push_op(CompiledRegex::Jump);
                goto_inner_end_offsets.push_back(alloc_offset());

                auto right_pos = compile_node(children[1]);
                get_offset(offset) = right_pos;

                break;
            }
            case ParsedRegex::LookAhead:
                push_op(CompiledRegex::LookAhead);
                push_string(node->children);
                break;
            case ParsedRegex::LookBehind:
                push_op(CompiledRegex::LookBehind);
                push_string(node->children, true);
                break;
            case ParsedRegex::NegativeLookAhead:
                push_op(CompiledRegex::NegativeLookAhead);
                push_string(node->children);
                break;
            case ParsedRegex::NegativeLookBehind:
                push_op(CompiledRegex::NegativeLookBehind);
                push_string(node->children, true);
                break;
            case ParsedRegex::LineStart:
                push_op(CompiledRegex::LineStart);
                break;
            case ParsedRegex::LineEnd:
                push_op(CompiledRegex::LineEnd);
                break;
            case ParsedRegex::WordBoundary:
                push_op(CompiledRegex::WordBoundary);
                break;
            case ParsedRegex::NotWordBoundary:
                push_op(CompiledRegex::NotWordBoundary);
                break;
            case ParsedRegex::SubjectBegin:
                push_op(CompiledRegex::SubjectBegin);
                break;
            case ParsedRegex::SubjectEnd:
                push_op(CompiledRegex::SubjectEnd);
                break;
            case ParsedRegex::ResetStart:
                push_op(CompiledRegex::Save);
                push_byte(0);
                break;
        }

        for (auto& offset : goto_inner_end_offsets)
            get_offset(offset) =  m_program.bytecode.size();

        if (capture != -1)
        {
            push_op(CompiledRegex::Save);
            push_byte(capture * 2 + 1);
        }

        return start_pos;
    }

    Offset compile_node(const ParsedRegex::AstNodePtr& node)
    {
        Offset pos = m_program.bytecode.size();
        Vector<Offset> goto_end_offsets;

        auto& quantifier = node->quantifier;

        if (quantifier.allows_none())
        {
            push_op(quantifier.greedy ? CompiledRegex::Split_PrioritizeParent
                                      : CompiledRegex::Split_PrioritizeChild);
            goto_end_offsets.push_back(alloc_offset());
        }

        auto inner_pos = compile_node_inner(node);
        // Write the node multiple times when we have a min count quantifier
        for (int i = 1; i < quantifier.min; ++i)
            inner_pos = compile_node_inner(node);

        if (quantifier.allows_infinite_repeat())
        {
            push_op(quantifier.greedy ? CompiledRegex::Split_PrioritizeChild
                                      : CompiledRegex::Split_PrioritizeParent);
            get_offset(alloc_offset()) = inner_pos;
        }
        // Write the node as an optional match for the min -> max counts
        else for (int i = std::max(1, quantifier.min); // STILL UGLY !
                  i < quantifier.max; ++i)
        {
            push_op(quantifier.greedy ? CompiledRegex::Split_PrioritizeParent
                                      : CompiledRegex::Split_PrioritizeChild);
            goto_end_offsets.push_back(alloc_offset());
            compile_node_inner(node);
        }

        for (auto offset : goto_end_offsets)
            get_offset(offset) = m_program.bytecode.size();

        return pos;
    }

    // Add a '.*' as the first instructions for the search use case
    void write_search_prefix()
    {
        kak_assert(m_program.bytecode.empty());
        push_op(CompiledRegex::Split_PrioritizeChild);
        get_offset(alloc_offset()) = CompiledRegex::search_prefix_size;
        push_op(CompiledRegex::AnyChar);
        push_op(CompiledRegex::Split_PrioritizeParent);
        get_offset(alloc_offset()) = 1 + sizeof(Offset);
    }

    Offset alloc_offset()
    {
        auto pos = m_program.bytecode.size();
        m_program.bytecode.resize(pos + sizeof(Offset));
        return pos;
    }

    Offset& get_offset(Offset pos)
    {
        return *reinterpret_cast<Offset*>(&m_program.bytecode[pos]);
    }

    void push_op(CompiledRegex::Op op)
    {
        m_program.bytecode.push_back(op);
    }

    void push_byte(char byte)
    {
        m_program.bytecode.push_back(byte);
    }

    void push_codepoint(Codepoint cp)
    {
        utf8::dump(std::back_inserter(m_program.bytecode), cp);
    }

    void push_string(const Vector<ParsedRegex::AstNodePtr>& codepoints, bool reversed = false)
    {
        if (codepoints.size() > 127)
            throw runtime_error{"Too long literal string"};

        push_byte(codepoints.size());
        if (reversed)
            for (auto& cp : codepoints | reverse()) 
                push_codepoint(cp->value);
        else
            for (auto& cp : codepoints) 
                push_codepoint(cp->value);
    }

    CompiledRegex m_program;
    const ParsedRegex& m_parsed_regex;
};

void dump_regex(const CompiledRegex& program)
{
    for (auto pos = program.bytecode.data(), end = program.bytecode.data() + program.bytecode.size();
         pos < end; )
    {
        printf("%4zd    ", pos - program.bytecode.data());
        const auto op = (CompiledRegex::Op)*pos++;
        switch (op)
        {
            case CompiledRegex::Literal:
                printf("literal %lc\n", utf8::read_codepoint(pos, (const char*)nullptr));
                break;
            case CompiledRegex::LiteralIgnoreCase:
                printf("literal (ignore case) %lc\n", utf8::read_codepoint(pos, (const char*)nullptr));
                break;
            case CompiledRegex::AnyChar:
                printf("any char\n");
                break;
            case CompiledRegex::Jump:
                printf("jump %u\n", *reinterpret_cast<const CompiledRegex::Offset*>(&*pos));
                pos += sizeof(CompiledRegex::Offset);
                break;
            case CompiledRegex::Split_PrioritizeParent:
            case CompiledRegex::Split_PrioritizeChild:
            {
                printf("split (prioritize %s) %u\n",
                       op == CompiledRegex::Split_PrioritizeParent ? "parent" : "child",
                       *reinterpret_cast<const CompiledRegex::Offset*>(&*pos));
                pos += sizeof(CompiledRegex::Offset);
                break;
            }
            case CompiledRegex::Save:
                printf("save %d\n", *pos++);
                break;
            case CompiledRegex::Matcher:
                printf("matcher %d\n", *pos++);
                break;
            case CompiledRegex::LineStart:
                printf("line start\n");
                break;
            case CompiledRegex::LineEnd:
                printf("line end\n");
                break;
            case CompiledRegex::WordBoundary:
                printf("word boundary\n");
                break;
            case CompiledRegex::NotWordBoundary:
                printf("not word boundary\n");
                break;
            case CompiledRegex::SubjectBegin:
                printf("subject begin\n");
                break;
            case CompiledRegex::SubjectEnd:
                printf("subject end\n");
                break;
            case CompiledRegex::LookAhead:
            case CompiledRegex::NegativeLookAhead:
            case CompiledRegex::LookBehind:
            case CompiledRegex::NegativeLookBehind:
            {
                int count = *pos++;
                StringView str{pos, pos + count};
                const char* name = nullptr;
                if (op == CompiledRegex::LookAhead)
                    name = "look ahead";
                if (op == CompiledRegex::NegativeLookAhead)
                    name = "negative look ahead";
                if (op == CompiledRegex::LookBehind)
                    name = "look behind";
                if (op == CompiledRegex::NegativeLookBehind)
                    name = "negative look behind";

                printf("%s (%s)\n", name, (const char*)str.zstr());
                pos += count;
                break;
            }
            case CompiledRegex::Match:
                printf("match\n");
        }
    }
}

CompiledRegex compile_regex(StringView re)
{
    CompiledRegex res;
    try
    {
        res = RegexCompiler::compile(re);
    }
    catch (runtime_error& err)
    {
        write_to_debug_buffer(err.what());
    }
    return std::move(res);
}

auto test_regex = UnitTest{[]{
    struct TestVM : CompiledRegex, ThreadedRegexVM<const char*>
    {
        TestVM(StringView re, bool dump = false)
            : CompiledRegex{RegexCompiler::compile(re)},
              ThreadedRegexVM{(const CompiledRegex&)*this}
        { if (dump) dump_regex(*this); }

        bool exec(StringView re, bool match = true, bool longest = false)
        {
            RegexExecFlags flags = RegexExecFlags::None;
            if (not match)
                flags |= RegexExecFlags::Search;
            if (not longest)
                flags |= RegexExecFlags::AnyMatch;

            return ThreadedRegexVM::exec(re.begin(), re.end(), flags);
        }
    };

    {
        TestVM vm{R"(a*b)"};
        kak_assert(vm.exec("b"));
        kak_assert(vm.exec("ab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("acb"));
        kak_assert(not vm.exec("abc"));
        kak_assert(not vm.exec(""));
    }

    {
        TestVM vm{R"(^a.*b$)"};
        kak_assert(vm.exec("afoob"));
        kak_assert(vm.exec("ab"));
        kak_assert(not vm.exec("bab"));
        kak_assert(not vm.exec(""));
    }

    {
        TestVM vm{R"(^(foo|qux|baz)+(bar)?baz$)"};
        kak_assert(vm.exec("fooquxbarbaz"));
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "qux");
        kak_assert(not vm.exec("fooquxbarbaze"));
        kak_assert(not vm.exec("quxbar"));
        kak_assert(not vm.exec("blahblah"));
        kak_assert(vm.exec("bazbaz"));
        kak_assert(vm.exec("quxbaz"));
    }

    {
        TestVM vm{R"(.*\b(foo|bar)\b.*)"};
        kak_assert(vm.exec("qux foo baz"));
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "foo");
        kak_assert(not vm.exec("quxfoobaz"));
        kak_assert(vm.exec("bar"));
        kak_assert(not vm.exec("foobar"));
    }

    {
        TestVM vm{R"((foo|bar))"};
        kak_assert(vm.exec("foo"));
        kak_assert(vm.exec("bar"));
        kak_assert(not vm.exec("foobar"));
    }

    {
        TestVM vm{R"(a{3,5}b)"};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaaaab"));
        kak_assert(vm.exec("aaaaab"));
    }

    {
        TestVM vm{R"(a{3}b)"};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaab"));
    }

    {
        TestVM vm{R"(a{3,}b)"};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(vm.exec("aaaaab"));
    }

    {
        TestVM vm{R"(a{,3}b)"};
        kak_assert(vm.exec("b"));
        kak_assert(vm.exec("ab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaab"));
    }

    {
        TestVM vm{R"(f.*a(.*o))"};
        kak_assert(vm.exec("blahfoobarfoobaz", false, true));
        kak_assert(StringView{vm.m_captures[0], vm.m_captures[1]} == "foobarfoo");
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "rfoo");
        kak_assert(vm.exec("mais que fais la police", false, true));
        kak_assert(StringView{vm.m_captures[0], vm.m_captures[1]} == "fais la po");
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == " po");
    }

    {
        TestVM vm{R"([àb-dX-Z-]{3,5})"};
        kak_assert(vm.exec("cà-Y"));
        kak_assert(not vm.exec("àeY"));
        kak_assert(vm.exec("dcbàX"));
        kak_assert(not vm.exec("efg"));
    }

    {
        TestVM vm{R"((a{3,5})a+)"};
        kak_assert(vm.exec("aaaaaa", true, true));
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "aaaaa");
    }

    {
        TestVM vm{R"((a{3,5}?)a+)"};
        kak_assert(vm.exec("aaaaaa", true, true));
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "aaa");
    }

    {
        TestVM vm{R"((a{3,5}?)a)"};
        kak_assert(vm.exec("aaaa"));
    }

    {
        TestVM vm{R"(\d{3})"};
        kak_assert(vm.exec("123"));
        kak_assert(not vm.exec("1x3"));
    }

    {
        TestVM vm{R"([-\d]+)"};
        kak_assert(vm.exec("123-456"));
        kak_assert(not vm.exec("123_456"));
    }

    {
        TestVM vm{R"([ \H]+)"};
        kak_assert(vm.exec("abc "));
        kak_assert(not vm.exec("a \t"));
    }

    {
        TestVM vm{R"(\Q{}[]*+?\Ea+)"};
        kak_assert(vm.exec("{}[]*+?aa"));
    }

    {
        TestVM vm{R"(\Q...)"};
        kak_assert(vm.exec("..."));
        kak_assert(not vm.exec("bla"));
    }

    {
        TestVM vm{R"(foo\Kbar)"};
        kak_assert(vm.exec("foobar", true, true));
        kak_assert(StringView{vm.m_captures[0], vm.m_captures[1]} == "bar");
        kak_assert(not vm.exec("bar", true, true));
    }

    {
        TestVM vm{R"((fo+?).*)"};
        kak_assert(vm.exec("foooo", true, true));
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "fo");
    }

    {
        TestVM vm{R"((?=foo).)"};
        kak_assert(vm.exec("barfoo", false, true));
        kak_assert(StringView{vm.m_captures[0], vm.m_captures[1]} == "f");
    }

    {
        TestVM vm{R"((?!foo)...)"};
        kak_assert(not vm.exec("foo"));
        kak_assert(vm.exec("qux"));
    }

    {
        TestVM vm{R"(...(?<=foo))"};
        kak_assert(vm.exec("foo"));
        kak_assert(not vm.exec("qux"));
    }

    {
        TestVM vm{R"(...(?<!foo))"};
        kak_assert(not vm.exec("foo"));
        kak_assert(vm.exec("qux"));
    }

    {
        TestVM vm{R"(Foo(?i)f[oB]+)"};
        kak_assert(vm.exec("FooFOoBb"));
    }

    {
        TestVM vm{R"([^\]]+)"};
        kak_assert(not vm.exec("a]c"));
        kak_assert(vm.exec("abc"));
    }
}};

}
