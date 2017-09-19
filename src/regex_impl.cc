#include "regex_impl.hh"
#include "vector.hh"
#include "unit_tests.hh"
#include "string.hh"
#include "unicode.hh"
#include "exception.hh"
#include "array_view.hh"

namespace Kakoune
{

namespace RegexProgram
{
enum Op : char
{
    Match,
    Literal,
    AnyChar,
    Jump,
    Split,
    LineStart,
    LineEnd,
    WordBoundary,
    NotWordBoundary,
    SubjectBegin,
    SubjectEnd,
};

using Offset = size_t;
}

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

AstNodePtr make_ast_node(Op op, char value = 0,
                         Quantifier quantifier = {Quantifier::One})
{
    return AstNodePtr{new AstNode{op, value, quantifier, {}}};
}

// Recursive descent parser based on naming using in the ECMAScript
// standard, although the syntax is not fully compatible.
template<typename Iterator>
struct Parser
{
    static AstNodePtr parse(Iterator pos, Iterator end)
    {
        return disjunction(pos, end);
    }

private:
    static AstNodePtr disjunction(Iterator& pos, Iterator end)
    {
        AstNodePtr node = alternative(pos, end);
        if (pos == end or *pos != '|')
            return node;

        AstNodePtr res = make_ast_node(Op::Alternation);
        res->children.push_back(std::move(node));
        res->children.push_back(disjunction(++pos, end));
        return res;
    }

    static AstNodePtr alternative(Iterator& pos, Iterator end)
    {
        AstNodePtr res = make_ast_node(Op::Sequence);
        while (auto node = term(pos, end))
            res->children.push_back(std::move(node));
        return res;
    }

    static AstNodePtr term(Iterator& pos, Iterator end)
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

    static AstNodePtr assertion(Iterator& pos, Iterator end)
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

    static AstNodePtr atom(Iterator& pos, Iterator end)
    {
        const auto c = *pos;
        switch (c)
        {
            case '.': ++pos; return make_ast_node(Op::AnyChar);
            case '(':
            {
                ++pos;
                auto content = disjunction(pos, end);
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

    static Quantifier quantifier(Iterator& pos, Iterator end)
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
};

RegexProgram::Offset compile_node(Vector<char>& program, const AstNodePtr& node);

RegexProgram::Offset alloc_offset(Vector<char>& instructions)
{
    auto pos = instructions.size();
    instructions.resize(instructions.size() + sizeof(RegexProgram::Offset));
    return pos;
}

RegexProgram::Offset& get_offset(Vector<char>& instructions, RegexProgram::Offset base)
{
    return *reinterpret_cast<RegexProgram::Offset*>(&instructions[base]);
}

RegexProgram::Offset compile_node_inner(Vector<char>& program, const AstNodePtr& node)
{
    const auto start_pos = program.size();

    Vector<RegexProgram::Offset> goto_inner_end_offsets;
    switch (node->op)
    {
        case Op::Literal:
            program.push_back(RegexProgram::Literal);
            program.push_back(node->value);
            break;
        case Op::AnyChar:
            program.push_back(RegexProgram::AnyChar);
            break;
        case Op::Sequence:
            for (auto& child : node->children)
                compile_node(program, child);
            break;
        case Op::Alternation:
        {
            auto& children = node->children;
            kak_assert(children.size() == 2);

            program.push_back(RegexProgram::Split);
            auto offset = alloc_offset(program);

            compile_node(program, children[0]);
            program.push_back(RegexProgram::Jump);
            goto_inner_end_offsets.push_back(alloc_offset(program));

            auto right_pos = compile_node(program, children[1]);
            get_offset(program, offset) = right_pos;

            break;
        }
        case Op::LineStart:
            program.push_back(RegexProgram::LineStart);
            break;
        case Op::LineEnd:
            program.push_back(RegexProgram::LineEnd);
            break;
        case Op::WordBoundary:
            program.push_back(RegexProgram::WordBoundary);
            break;
        case Op::NotWordBoundary:
            program.push_back(RegexProgram::NotWordBoundary);
            break;
        case Op::SubjectBegin:
            program.push_back(RegexProgram::SubjectBegin);
            break;
        case Op::SubjectEnd:
            program.push_back(RegexProgram::SubjectEnd);
            break;
    }

    for (auto& offset : goto_inner_end_offsets)
        get_offset(program, offset) =  program.size();

    return start_pos;
}

RegexProgram::Offset compile_node(Vector<char>& program, const AstNodePtr& node)
{
    RegexProgram::Offset pos = program.size();
    Vector<RegexProgram::Offset> goto_end_offsets;

    if (node->quantifier.allows_none())
    {
        program.push_back(RegexProgram::Split);
        goto_end_offsets.push_back(alloc_offset(program));
    }

    auto inner_pos = compile_node_inner(program, node);
    // Write the node multiple times when we have a min count quantifier
    for (int i = 1; i < node->quantifier.min; ++i)
        inner_pos = compile_node_inner(program, node);

    if (node->quantifier.allows_infinite_repeat())
    {
        program.push_back(RegexProgram::Split);
        get_offset(program, alloc_offset(program)) = inner_pos;
    }
    // Write the node as an optional match for the min -> max counts
    else for (int i = std::max(1, node->quantifier.min); // STILL UGLY !
              i < node->quantifier.max; ++i)
    {
        program.push_back(RegexProgram::Split);
        goto_end_offsets.push_back(alloc_offset(program));
        compile_node_inner(program, node);
    }

    for (auto offset : goto_end_offsets)
        get_offset(program, offset) = program.size();

    return pos;
}

Vector<char> compile(const AstNodePtr& node)
{
    Vector<char> res;
    compile_node(res, node);
    res.push_back(RegexProgram::Match);
    return res;
}

template<typename Iterator>
Vector<char> compile(Iterator begin, Iterator end)
{
    return compile(Parser<Iterator>::parse(begin, end));
}
}

namespace RegexProgram
{
void dump(ConstArrayView<char> program)
{
    for (size_t pos = 0; pos < program.size(); )
    {
        printf("%4zd    ", pos);
        switch ((RegexProgram::Op)program[pos++])
        {
            case RegexProgram::Literal:
                printf("literal %c\n", program[pos++]);
                break;
            case RegexProgram::AnyChar:
                printf("any char\n");
                break;
            case RegexProgram::Jump:
                printf("jump %zd\n", *reinterpret_cast<const RegexProgram::Offset*>(&program[pos]));
                pos += sizeof(RegexProgram::Offset);
                break;
            case RegexProgram::Split:
            {
                printf("split %zd\n", *reinterpret_cast<const RegexProgram::Offset*>(&program[pos]));
                pos += sizeof(RegexProgram::Offset);
                break;
            }
            case RegexProgram::LineStart:
                printf("line start\n");
                break;
            case RegexProgram::LineEnd:
                printf("line end\n");
                break;
            case RegexProgram::WordBoundary:
                printf("word boundary\n");
                break;
            case RegexProgram::NotWordBoundary:
                printf("not word boundary\n");
                break;
            case RegexProgram::SubjectBegin:
                printf("subject begin\n");
                break;
            case RegexProgram::SubjectEnd:
                printf("subject end\n");
                break;
            case RegexProgram::Match:
                printf("match\n");
        }
    }
}

struct ThreadedExecutor
{
    ThreadedExecutor(ConstArrayView<char> program) : m_program{program} {}

    struct StepResult
    {
        enum Result { Consumed, Matched, Failed } result;
        const char* next = nullptr;
    };

    StepResult step(const char* inst)
    {
        while (true)
        {
            char c = m_pos == m_subject.end() ? 0 : *m_pos;
            const RegexProgram::Op op = (RegexProgram::Op)*inst++;
            switch (op)
            {
                case RegexProgram::Literal:
                    if (*inst++ == c)
                        return { StepResult::Consumed, inst };
                    return { StepResult::Failed };
                case RegexProgram::AnyChar:
                    return { StepResult::Consumed, inst };
                case RegexProgram::Jump:
                    inst = m_program.begin() + *reinterpret_cast<const RegexProgram::Offset*>(inst);
                    // if instruction is already going to be executed, drop this thread
                    if (std::find(m_threads.begin(), m_threads.end(), inst) != m_threads.end())
                        return { StepResult::Failed };
                    break;
                case RegexProgram::Split:
                {
                    add_thread(*reinterpret_cast<const RegexProgram::Offset*>(inst));
                    inst += sizeof(RegexProgram::Offset);
                    break;
                }
                case RegexProgram::LineStart:
                    if (not is_line_start())
                        return { StepResult::Failed };
                    break;
                case RegexProgram::LineEnd:
                    if (not is_line_end())
                        return { StepResult::Failed };
                    break;
                case RegexProgram::WordBoundary:
                    if (not is_word_boundary())
                        return { StepResult::Failed };
                    break;
                case RegexProgram::NotWordBoundary:
                    if (is_word_boundary())
                        return { StepResult::Failed };
                    break;
                case RegexProgram::SubjectBegin:
                    if (m_pos != m_subject.begin())
                        return { StepResult::Failed };
                    break;
                case RegexProgram::SubjectEnd:
                    if (m_pos != m_subject.end())
                        return { StepResult::Failed };
                    break;
                case RegexProgram::Match:
                    return { StepResult::Matched };
            }
        }
        return { StepResult::Failed };
    }

    bool match(ConstArrayView<char> program, StringView data)
    {
        m_threads = Vector<const char*>{program.begin()};
        m_subject = data;
        m_pos = data.begin();

        for (m_pos = m_subject.begin(); m_pos != m_subject.end(); ++m_pos)
        {
            for (int i = 0; i < m_threads.size(); ++i)
            {
                auto res = step(m_threads[i]);
                m_threads[i] = res.next;
                if (res.result == StepResult::Matched)
                    return true;
            }
            m_threads.erase(std::remove(m_threads.begin(), m_threads.end(), nullptr), m_threads.end());
            if (m_threads.empty())
                break;
        }

        // Step remaining threads to see if they match without consuming anything else
        for (int i = 0; i < m_threads.size(); ++i)
        {
            if (step(m_threads[i]).result == StepResult::Matched)
                return true;
        }
        return false;
    }

    void add_thread(RegexProgram::Offset pos)
    {
        const char* inst = m_program.begin() + pos;
        if (std::find(m_threads.begin(), m_threads.end(), inst) == m_threads.end())
            m_threads.push_back(inst);
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

    ConstArrayView<char> m_program;
    Vector<const char*> m_threads;
    StringView m_subject;
    const char* m_pos;
};
}

auto test_regex = UnitTest{[]{
    using Exec = RegexProgram::ThreadedExecutor;
    {
        StringView re = R"(a*b)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(exec.match(program, "b"));
        kak_assert(exec.match(program, "ab"));
        kak_assert(exec.match(program, "aaab"));
        kak_assert(not exec.match(program, "acb"));
        kak_assert(not exec.match(program, ""));
    }

    {
        StringView re = R"(^a.*b$)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(exec.match(program, "afoob"));
        kak_assert(exec.match(program, "ab"));
        kak_assert(not exec.match(program, "bab"));
        kak_assert(not exec.match(program, ""));
    }

    {
        StringView re = R"(^(foo|qux|baz)+(bar)?baz$)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(exec.match(program, "fooquxbarbaz"));
        kak_assert(not exec.match(program, "fooquxbarbaze"));
        kak_assert(not exec.match(program, "quxbar"));
        kak_assert(not exec.match(program, "blahblah"));
        kak_assert(exec.match(program, "bazbaz"));
        kak_assert(exec.match(program, "quxbaz"));
    }

    {
        StringView re = R"(.*\b(foo|bar)\b.*)";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(exec.match(program, "qux foo baz"));
        kak_assert(not exec.match(program, "quxfoobaz"));
        kak_assert(exec.match(program, "bar"));
        kak_assert(not exec.match(program, "foobar"));
    }
    {
        StringView re = R"(\`(foo|bar)\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(exec.match(program, "foo"));
        kak_assert(exec.match(program, "bar"));
        kak_assert(not exec.match(program, "foobar"));
    }

    {
        StringView re = R"(\`a{3,5}b\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(not exec.match(program, "aab"));
        kak_assert(exec.match(program, "aaab"));
        kak_assert(not exec.match(program, "aaaaaab"));
        kak_assert(exec.match(program, "aaaaab"));
    }

    {
        StringView re = R"(\`a{3,}b\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(not exec.match(program, "aab"));
        kak_assert(exec.match(program, "aaab"));
        kak_assert(exec.match(program, "aaaaab"));
    }

    {
        StringView re = R"(\`a{,3}b\')";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(exec.match(program, "b"));
        kak_assert(exec.match(program, "ab"));
        kak_assert(exec.match(program, "aaab"));
        kak_assert(not exec.match(program, "aaaab"));
    }
}};

}
