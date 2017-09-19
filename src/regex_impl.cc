#include "regex_impl.hh"
#include "vector.hh"
#include "unit_tests.hh"
#include "string.hh"
#include "unicode.hh"
#include "exception.hh"
#include "array_view.hh"

namespace Kakoune
{

struct CompiledRegex
{
    enum Op : char
    {
        Match,
        Literal,
        AnyChar,
        Jump,
        Split,
        Save,
        LineStart,
        LineEnd,
        WordBoundary,
        NotWordBoundary,
        SubjectBegin,
        SubjectEnd,
    };

    using Offset = unsigned;

    Vector<char> bytecode;
    size_t save_count;
};

namespace RegexCompiler
{

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

enum class Op
{
    Literal,
    AnyChar,
    Sequence,
    Alternation,
    LineStart,
    LineEnd,
    WordBoundary,
    NotWordBoundary,
    SubjectBegin,
    SubjectEnd,
};

struct AstNode
{
    Op op;
    char value;
    Quantifier quantifier;
    Vector<std::unique_ptr<AstNode>> children;
};

using AstNodePtr = std::unique_ptr<AstNode>;

AstNodePtr make_ast_node(Op op, char value = -1,
                         Quantifier quantifier = {Quantifier::One})
{
    return AstNodePtr{new AstNode{op, value, quantifier, {}}};
}

// Recursive descent parser based on naming using in the ECMAScript
// standard, although the syntax is not fully compatible.
template<typename Iterator>
struct Parser
{
    AstNodePtr parse(Iterator pos, Iterator end)
    {
        return disjunction(pos, end, 0);
    }

    size_t capture_count() const { return m_next_capture; }

private:
    AstNodePtr disjunction(Iterator& pos, Iterator end, char capture = -1)
    {
        AstNodePtr node = alternative(pos, end);
        if (pos == end or *pos != '|')
        {
            node->value = capture;
            return node;
        }

        AstNodePtr res = make_ast_node(Op::Alternation);
        res->children.push_back(std::move(node));
        res->children.push_back(disjunction(++pos, end));
        res->value = capture;
        return res;
    }

    AstNodePtr alternative(Iterator& pos, Iterator end)
    {
        AstNodePtr res = make_ast_node(Op::Sequence);
        while (auto node = term(pos, end))
            res->children.push_back(std::move(node));
        return res;
    }

    AstNodePtr term(Iterator& pos, Iterator end)
    {
        if (auto node = assertion(pos, end))
            return node;
        if (auto node = atom(pos, end))
        {
            node->quantifier = quantifier(pos, end);
            return node;
        }
        return nullptr;
    }

    AstNodePtr assertion(Iterator& pos, Iterator end)
    {
        switch (*pos)
        {
            case '^': ++pos; return make_ast_node(Op::LineStart);
            case '$': ++pos; return make_ast_node(Op::LineEnd);
            case '\\':
                if (pos+1 == end)
                    return nullptr;
                switch (*(pos+1))
                {
                    case 'b': pos += 2; return make_ast_node(Op::WordBoundary);
                    case 'B': pos += 2; return make_ast_node(Op::NotWordBoundary);
                    case '`': pos += 2; return make_ast_node(Op::SubjectBegin);
                    case '\'': pos += 2; return make_ast_node(Op::SubjectEnd);
                }
                break;
            /* TODO: look ahead, look behind */
        }
        return nullptr;
    }

    AstNodePtr atom(Iterator& pos, Iterator end)
    {
        const auto c = *pos;
        switch (c)
        {
            case '.': ++pos; return make_ast_node(Op::AnyChar);
            case '(':
            {
                ++pos;
                auto content = disjunction(pos, end, m_next_capture++);

                if (pos == end or *pos != ')')
                    throw runtime_error{"Unclosed parenthesis"};
                ++pos;
                return content;
            }
            default:
                if (contains("^$.*+?()[]{}|", c))
                    return nullptr;
                ++pos;
                return make_ast_node(Op::Literal, c);
        }
    }

    Quantifier quantifier(Iterator& pos, Iterator end)
    {
        auto read_int = [](Iterator& pos, Iterator begin, Iterator end) {
            int res = 0;
            for (; pos != end; ++pos)
            {
                const auto c = *pos;
                if (c < '0' or c > '9')
                    return pos == begin ? -1 : res;
                res = res * 10 + c - '0';
            }
            return res;
        };

        switch (*pos)
        {
            case '*': ++pos; return {Quantifier::RepeatZeroOrMore};
            case '+': ++pos; return {Quantifier::RepeatOneOrMore};
            case '?': ++pos; return {Quantifier::Optional};
            case '{':
            {
                auto it = pos+1;
                int min = read_int(it, it, end);
                int max = -1;
                if (*it == ',')
                {
                    ++it;
                    max = read_int(it, it, end);
                }
                if (*it++ != '}')
                   throw runtime_error{"expected closing bracket"};
                pos = it;
                return {Quantifier::RepeatMinMax, min, max};
            }
            default: return {Quantifier::One};
        }
    }

    char m_next_capture = 1;
};

CompiledRegex::Offset compile_node(CompiledRegex& program, const AstNodePtr& node);

CompiledRegex::Offset alloc_offset(CompiledRegex& program)
{
    auto pos = program.bytecode.size();
    program.bytecode.resize(pos + sizeof(CompiledRegex::Offset));
    return pos;
}

CompiledRegex::Offset& get_offset(CompiledRegex& program, CompiledRegex::Offset pos)
{
    return *reinterpret_cast<CompiledRegex::Offset*>(&program.bytecode[pos]);
}

CompiledRegex::Offset compile_node_inner(CompiledRegex& program, const AstNodePtr& node)
{
    const auto start_pos = program.bytecode.size();

    const char capture = (node->op == Op::Alternation or node->op == Op::Sequence) ? node->value : -1;
    if (capture >= 0)
    {
        program.bytecode.push_back(CompiledRegex::Save);
        program.bytecode.push_back(capture * 2);
    }

    Vector<CompiledRegex::Offset> goto_inner_end_offsets;
    switch (node->op)
    {
        case Op::Literal:
            program.bytecode.push_back(CompiledRegex::Literal);
            program.bytecode.push_back(node->value);
            break;
        case Op::AnyChar:
            program.bytecode.push_back(CompiledRegex::AnyChar);
            break;
        case Op::Sequence:
            for (auto& child : node->children)
                compile_node(program, child);
            break;
        case Op::Alternation:
        {
            auto& children = node->children;
            kak_assert(children.size() == 2);

            program.bytecode.push_back(CompiledRegex::Split);
            auto offset = alloc_offset(program);

            compile_node(program, children[0]);
            program.bytecode.push_back(CompiledRegex::Jump);
            goto_inner_end_offsets.push_back(alloc_offset(program));

            auto right_pos = compile_node(program, children[1]);
            get_offset(program, offset) = right_pos;

            break;
        }
        case Op::LineStart:
            program.bytecode.push_back(CompiledRegex::LineStart);
            break;
        case Op::LineEnd:
            program.bytecode.push_back(CompiledRegex::LineEnd);
            break;
        case Op::WordBoundary:
            program.bytecode.push_back(CompiledRegex::WordBoundary);
            break;
        case Op::NotWordBoundary:
            program.bytecode.push_back(CompiledRegex::NotWordBoundary);
            break;
        case Op::SubjectBegin:
            program.bytecode.push_back(CompiledRegex::SubjectBegin);
            break;
        case Op::SubjectEnd:
            program.bytecode.push_back(CompiledRegex::SubjectEnd);
            break;
    }

    for (auto& offset : goto_inner_end_offsets)
        get_offset(program, offset) =  program.bytecode.size();

    if (capture >= 0)
    {
        program.bytecode.push_back(CompiledRegex::Save);
        program.bytecode.push_back(capture * 2 + 1);
    }

    return start_pos;
}

CompiledRegex::Offset compile_node(CompiledRegex& program, const AstNodePtr& node)
{
    CompiledRegex::Offset pos = program.bytecode.size();
    Vector<CompiledRegex::Offset> goto_end_offsets;

    if (node->quantifier.allows_none())
    {
        program.bytecode.push_back(CompiledRegex::Split);
        goto_end_offsets.push_back(alloc_offset(program));
    }

    auto inner_pos = compile_node_inner(program, node);
    // Write the node multiple times when we have a min count quantifier
    for (int i = 1; i < node->quantifier.min; ++i)
        inner_pos = compile_node_inner(program, node);

    if (node->quantifier.allows_infinite_repeat())
    {
        program.bytecode.push_back(CompiledRegex::Split);
        get_offset(program, alloc_offset(program)) = inner_pos;
    }
    // Write the node as an optional match for the min -> max counts
    else for (int i = std::max(1, node->quantifier.min); // STILL UGLY !
              i < node->quantifier.max; ++i)
    {
        program.bytecode.push_back(CompiledRegex::Split);
        goto_end_offsets.push_back(alloc_offset(program));
        compile_node_inner(program, node);
    }

    for (auto offset : goto_end_offsets)
        get_offset(program, offset) = program.bytecode.size();

    return pos;
}

constexpr CompiledRegex::Offset prefix_size = 3 + 2 * sizeof(CompiledRegex::Offset);

// Add a '.*' as the first instructions for the search use case
void write_search_prefix(CompiledRegex& program)
{
    kak_assert(program.bytecode.empty());
    program.bytecode.push_back(CompiledRegex::Split);
    get_offset(program, alloc_offset(program)) = prefix_size;
    program.bytecode.push_back(CompiledRegex::AnyChar);
    program.bytecode.push_back(CompiledRegex::Split);
    get_offset(program, alloc_offset(program)) = 1 + sizeof(CompiledRegex::Offset);
}

CompiledRegex compile(const AstNodePtr& node, size_t capture_count)
{
    CompiledRegex res;
    write_search_prefix(res);
    compile_node(res, node);
    res.bytecode.push_back(CompiledRegex::Match);
    res.save_count = capture_count * 2;
    return res;
}

template<typename Iterator>
CompiledRegex compile(Iterator begin, Iterator end)
{
    Parser<Iterator> parser;
    auto node = parser.parse(begin, end);
    return compile(node, parser.capture_count());
}

}

void dump(const CompiledRegex& program)
{
    for (auto pos = program.bytecode.begin(); pos < program.bytecode.end(); )
    {
        printf("%4zd    ", pos - program.bytecode.begin());
        switch ((CompiledRegex::Op)*pos++)
        {
            case CompiledRegex::Literal:
                printf("literal %c\n", *pos++);
                break;
            case CompiledRegex::AnyChar:
                printf("any char\n");
                break;
            case CompiledRegex::Jump:
                printf("jump %u\n", *reinterpret_cast<const CompiledRegex::Offset*>(&*pos));
                pos += sizeof(CompiledRegex::Offset);
                break;
            case CompiledRegex::Split:
            {
                printf("split %u\n", *reinterpret_cast<const CompiledRegex::Offset*>(&*pos));
                pos += sizeof(CompiledRegex::Offset);
                break;
            }
            case CompiledRegex::Save:
                printf("save %d\n", *pos++);
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
            case CompiledRegex::Match:
                printf("match\n");
        }
    }
}

struct ThreadedRegexVM
{
    ThreadedRegexVM(const CompiledRegex& program) : m_program{program} {}

    struct Thread
    {
        const char* inst;
        Vector<const char*> saves = {};
    };

    enum class StepResult { Consumed, Matched, Failed };
    StepResult step(size_t thread_index)
    {
        while (true)
        {
            auto& thread = m_threads[thread_index];
            char c = m_pos == m_subject.end() ? 0 : *m_pos;
            const CompiledRegex::Op op = (CompiledRegex::Op)*thread.inst++;
            switch (op)
            {
                case CompiledRegex::Literal:
                    if (*thread.inst++ == c)
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::AnyChar:
                    return StepResult::Consumed;
                case CompiledRegex::Jump:
                {
                    auto inst = m_program.bytecode.data() + *reinterpret_cast<const CompiledRegex::Offset*>(thread.inst);
                    // if instruction is already going to be executed by another thread, drop this thread
                    if (std::find_if(m_threads.begin(), m_threads.end(),
                                     [inst](const Thread& t) { return t.inst == inst; }) != m_threads.end())
                        return StepResult::Failed;
                    thread.inst = inst;
                    break;
                }
                case CompiledRegex::Split:
                {
                    add_thread(*reinterpret_cast<const CompiledRegex::Offset*>(thread.inst), thread.saves);
                    // thread is invalidated now, as we mutated the m_thread vector
                    m_threads[thread_index].inst += sizeof(CompiledRegex::Offset);
                    break;
                }
                case CompiledRegex::Save:
                {
                    const char index = *thread.inst++;
                    thread.saves[index] = m_pos;
                    break;
                }
                case CompiledRegex::LineStart:
                    if (not is_line_start())
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LineEnd:
                    if (not is_line_end())
                        return StepResult::Failed;
                    break;
                case CompiledRegex::WordBoundary:
                    if (not is_word_boundary())
                        return StepResult::Failed;
                    break;
                case CompiledRegex::NotWordBoundary:
                    if (is_word_boundary())
                        return StepResult::Failed;
                    break;
                case CompiledRegex::SubjectBegin:
                    if (m_pos != m_subject.begin())
                        return StepResult::Failed;
                    break;
                case CompiledRegex::SubjectEnd:
                    if (m_pos != m_subject.end())
                        return StepResult::Failed;
                    break;
                case CompiledRegex::Match:
                    return StepResult::Matched;
            }
        }
        return StepResult::Failed;
    }

    bool exec(StringView data, bool match = true)
    {
        m_threads.clear();
        add_thread(match ? RegexCompiler::prefix_size : 0,
                   Vector<const char*>(m_program.save_count, nullptr));

        m_subject = data;
        m_pos = data.begin();

        for (m_pos = m_subject.begin(); m_pos != m_subject.end(); ++m_pos)
        {
            for (int i = 0; i < m_threads.size(); ++i)
            {
                const auto res = step(i);
                if (res == StepResult::Matched)
                {
                    m_captures = std::move(m_threads[i].saves);
                    return true;
                }
                else if (res == StepResult::Failed)
                    m_threads[i].inst = nullptr;
            }
            m_threads.erase(std::remove_if(m_threads.begin(), m_threads.end(),
                                           [](const Thread& t) { return t.inst == nullptr; }), m_threads.end());
            if (m_threads.empty())
                return false;
        }

        // Step remaining threads to see if they match without consuming anything else
        for (int i = 0; i < m_threads.size(); ++i)
        {
            if (step(i) == StepResult::Matched)
            {
                m_captures = std::move(m_threads[i].saves);
                return true;
            }
        }
        return false;
    }

    void add_thread(CompiledRegex::Offset pos, Vector<const char*> saves)
    {
        const char* inst = m_program.bytecode.data() + pos;
        if (std::find_if(m_threads.begin(), m_threads.end(),
                         [inst](const Thread& t) { return t.inst == inst; }) == m_threads.end())
            m_threads.push_back({inst, std::move(saves)});
    }

    bool is_line_start() const
    {
        return m_pos == m_subject.begin() or *(m_pos-1) == '\n';
    }

    bool is_line_end() const
    {
        return m_pos == m_subject.end() or *m_pos == '\n';
    }

    bool is_word_boundary() const
    {
        return m_pos == m_subject.begin() or
               m_pos == m_subject.end() or
               is_word(*(m_pos-1)) != is_word(*m_pos);
    }

    const CompiledRegex& m_program;
    Vector<Thread> m_threads;
    Vector<const char*> m_captures;
    StringView m_subject;
    const char* m_pos;
};

auto test_regex = UnitTest{[]{
    {
        StringView re = R"(a*b)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(vm.exec("b"));
        kak_assert(vm.exec("ab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("acb"));
        kak_assert(not vm.exec(""));
    }

    {
        StringView re = R"(^a.*b$)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(vm.exec("afoob"));
        kak_assert(vm.exec("ab"));
        kak_assert(not vm.exec("bab"));
        kak_assert(not vm.exec(""));
    }

    {
        StringView re = R"(^(foo|qux|baz)+(bar)?baz$)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(vm.exec("fooquxbarbaz"));
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "qux");
        kak_assert(not vm.exec("fooquxbarbaze"));
        kak_assert(not vm.exec("quxbar"));
        kak_assert(not vm.exec("blahblah"));
        kak_assert(vm.exec("bazbaz"));
        kak_assert(vm.exec("quxbaz"));
    }

    {
        StringView re = R"(.*\b(foo|bar)\b.*)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(vm.exec("qux foo baz"));
        kak_assert(StringView{vm.m_captures[2], vm.m_captures[3]} == "foo");
        kak_assert(not vm.exec("quxfoobaz"));
        kak_assert(vm.exec("bar"));
        kak_assert(not vm.exec("foobar"));
    }
    {
        StringView re = R"(\`(foo|bar)\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(vm.exec("foo"));
        kak_assert(vm.exec("bar"));
        kak_assert(not vm.exec("foobar"));
    }

    {
        StringView re = R"(\`a{3,5}b\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaaaab"));
        kak_assert(vm.exec("aaaaab"));
    }

    {
        StringView re = R"(\`a{3,}b\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(not vm.exec("aab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(vm.exec("aaaaab"));
    }

    {
        StringView re = R"(\`a{,3}b\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(vm.exec("b"));
        kak_assert(vm.exec("ab"));
        kak_assert(vm.exec("aaab"));
        kak_assert(not vm.exec("aaaab"));
    }

    {
        StringView re = R"(f.*a)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        dump(program);
        ThreadedRegexVM vm{program};
        kak_assert(vm.exec("blahfoobarfoobaz", false));
        kak_assert(StringView{vm.m_captures[0], vm.m_captures[1]} == "fooba"); // TODO: leftmost, longest
        kak_assert(vm.exec("mais que fais la police", false));
        kak_assert(StringView{vm.m_captures[0], vm.m_captures[1]} == "fa");
    }
}};

}
