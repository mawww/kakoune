#include "regex_impl.hh"
#include "vector.hh"
#include "unit_tests.hh"
#include "string.hh"
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
};

using Offset = size_t;
}

namespace RegexCompiler
{
enum class Quantifier
{
    One,
    Optional,
    RepeatZeroOrMore,
    RepeatOneOrMore
};

enum class Op
{
    Literal,
    AnyChar,
    Sequence,
    Alternation,
    LineStart,
    LineEnd,
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
                         Quantifier quantifier = Quantifier::One)
{
    return AstNodePtr{new AstNode{op, value, quantifier, {}}};
}

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
        while (pos != end and *pos == '|')
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
            /* TODO: \`, \', \b, \B, look ahead, look behind */
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
        switch (*pos)
        {
            case '*': ++pos; return Quantifier::RepeatZeroOrMore;
            case '+': ++pos; return Quantifier::RepeatOneOrMore;
            case '?': ++pos; return Quantifier::Optional;
            default: return Quantifier::One;
        }
    }
};

RegexProgram::Offset compile_node(Vector<char>& program, const AstNodePtr& node)
{
    RegexProgram::Offset pos = program.size();

    auto allow_none = [](Quantifier quantifier) {
        return quantifier == Quantifier::Optional or
               quantifier == Quantifier::RepeatZeroOrMore;
    };

    auto is_repeat = [](Quantifier quantifier) {
        return quantifier == Quantifier::RepeatZeroOrMore or
               quantifier == Quantifier::RepeatOneOrMore;
    };

    auto alloc_offsets = [](Vector<char>& instructions, int count) {
        auto pos = instructions.size();
        instructions.resize(instructions.size() + count * sizeof(RegexProgram::Offset));
        return pos;
    };

    auto get_offset = [](Vector<char>& instructions, RegexProgram::Offset base, int index = 0) {
        return reinterpret_cast<RegexProgram::Offset*>(&instructions[base]) + index;
    };

    RegexProgram::Offset optional_offset = -1;
    if (allow_none(node->quantifier))
    {
        program.push_back(RegexProgram::Split);
        program.push_back(2);
        auto offsets = alloc_offsets(program, 2);
        *get_offset(program, offsets) = program.size();
        optional_offset = offsets;
    }

    Vector<RegexProgram::Offset> goto_end_offsets;
    auto content_pos = program.size();
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
            const auto count = node->children.size();
            if (count > 255)
                throw runtime_error{"More than 255 elements in an alternation is not supported"};

            program.push_back(RegexProgram::Split);
            program.push_back(count);
            auto offsets = alloc_offsets(program, count);
            auto& children = node->children;
            for (int i = 0; i < children.size(); ++i)
            {
                auto child_pos = compile_node(program, children[i]);
                *get_offset(program, offsets, i) = child_pos;
                // Jump to end after executing that children
                program.push_back(RegexProgram::Jump);
                goto_end_offsets.push_back(alloc_offsets(program, 1));
            }
            break;
        }
        case Op::LineStart:
            program.push_back(RegexProgram::LineStart);
            break;
        case Op::LineEnd:
            program.push_back(RegexProgram::LineEnd);
            break;
    }

    for (auto& offset : goto_end_offsets)
        *get_offset(program, offset) =  program.size();

    if (is_repeat(node->quantifier))
    {
        program.push_back(RegexProgram::Split);
        program.push_back(2);
        auto offsets = alloc_offsets(program, 2);
        *get_offset(program, offsets, 0) = content_pos;
        *get_offset(program, offsets, 1) = program.size();
    }

    if (optional_offset != -1)
        *get_offset(program, optional_offset, 1) = program.size();

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
                int count = program[pos++];
                printf("split [");
                for (int i = 0; i < count; ++i)
                    printf("%zd%s", reinterpret_cast<const RegexProgram::Offset*>(&program[pos])[i],
                           (i == count - 1) ? "]\n" : ", ");
                pos += count * sizeof(RegexProgram::Offset);
                break;
            }
            case RegexProgram::LineStart:
                printf("line start\n");
                break;
            case RegexProgram::LineEnd:
                printf("line end\n");
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
            auto c = m_pos == m_subject.end() ? 0 : *m_pos;
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
                    break;
                case RegexProgram::Split:
                {
                    const int count = *inst++;
                    auto* offsets = reinterpret_cast<const RegexProgram::Offset*>(inst);
                    for (int o = 1; o < count; ++o)
                        m_threads.push_back(m_program.begin() + offsets[o]);
                    inst = m_program.begin() + offsets[0];
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

    bool is_line_start() const
    {
        return m_pos == m_subject.begin() or *(m_pos-1) == '\n';
    }

    bool is_line_end() const
    {
        return m_pos == m_subject.end() or *m_pos == '\n';
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
        StringView re = "a*b";
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
        StringView re = "^(foo|qux)+(bar)?baz$";
        auto program = RegexCompiler::compile(re.begin(), re.end());
        RegexProgram::dump(program);
        Exec exec{program};
        kak_assert(exec.match(program, "fooquxbarbaz"));
        kak_assert(not exec.match(program, "quxbar"));
        kak_assert(not exec.match(program, "blahblah"));
        kak_assert(exec.match(program, "foobaz"));
        kak_assert(exec.match(program, "quxbaz"));
    }
}};

}
