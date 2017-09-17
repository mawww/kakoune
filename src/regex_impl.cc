#include "regex_impl.hh"
#include "vector.hh"
#include "unit_tests.hh"
#include "string.hh"
#include "exception.hh"

namespace Kakoune
{

struct RegexProgram
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
    using Instructions = Vector<char>;

    Instructions instructions;
};


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

    RegexProgram::Offset compile_node(RegexProgram& program, const AstNodePtr& node)
    {
        auto& insts = program.instructions;
        RegexProgram::Offset pos = insts.size();

        auto allow_none = [](Quantifier quantifier) {
            return quantifier == Quantifier::Optional or
                   quantifier == Quantifier::RepeatZeroOrMore;
        };

        auto is_repeat = [](Quantifier quantifier) {
            return quantifier == Quantifier::RepeatZeroOrMore or
                   quantifier == Quantifier::RepeatOneOrMore;
        };

        auto alloc_offsets = [](RegexProgram::Instructions& instructions, int count) {
            auto pos = instructions.size();
            instructions.resize(instructions.size() + count * sizeof(RegexProgram::Offset));
            return pos;
        };

        auto get_offset = [](RegexProgram::Instructions& instructions, RegexProgram::Offset base, int index = 0) {
            return reinterpret_cast<RegexProgram::Offset*>(&instructions[base]) + index;
        };

        RegexProgram::Offset optional_offset = -1;
        if (allow_none(node->quantifier))
        {
            insts.push_back(RegexProgram::Split);
            insts.push_back(2);
            auto offsets = alloc_offsets(insts, 2);
            *get_offset(insts, offsets) = insts.size();
            optional_offset = offsets;
        }

        Vector<RegexProgram::Offset> goto_end_offsets;
        auto content_pos = insts.size();
        switch (node->op)
        {
            case Op::Literal:
                insts.push_back(RegexProgram::Literal);
                insts.push_back(node->value);
                break;
            case Op::AnyChar:
                insts.push_back(RegexProgram::AnyChar);
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

                insts.push_back(RegexProgram::Split);
                insts.push_back(count);
                auto offsets = alloc_offsets(insts, count);
                auto& children = node->children;
                for (int i = 0; i < children.size(); ++i)
                {
                    auto child_pos = compile_node(program, children[i]);
                    *get_offset(insts, offsets, i) = child_pos;
                    // Jump to end after executing that children
                    insts.push_back(RegexProgram::Jump);
                    goto_end_offsets.push_back(alloc_offsets(insts, 1));
                }
                break;
            }
            case Op::LineStart:
                insts.push_back(RegexProgram::LineStart);
                break;
            case Op::LineEnd:
                insts.push_back(RegexProgram::LineEnd);
                break;
        }

        for (auto& offset : goto_end_offsets)
            *get_offset(insts, offset) =  insts.size();

        if (is_repeat(node->quantifier))
        {
            insts.push_back(RegexProgram::Split);
            insts.push_back(2);
            auto offsets = alloc_offsets(insts, 2);
            *get_offset(insts, offsets, 0) = content_pos;
            *get_offset(insts, offsets, 1) = insts.size();
        }

        if (optional_offset != -1)
            *get_offset(insts, optional_offset, 1) = insts.size();

        return pos;
    }

    RegexProgram compile(const AstNodePtr& node)
    {
        RegexProgram res;
        compile_node(res, node);
        res.instructions.push_back(RegexProgram::Match);
        return res;
    }
}

void dump_program(const RegexProgram& program)
{
    auto& insts = program.instructions;
    for (size_t pos = 0; pos < insts.size(); )
    {
        printf("%4zd    ", pos);
        switch ((RegexProgram::Op)insts[pos++])
        {
            case RegexProgram::Literal:
                printf("literal %c\n", insts[pos++]);
                break;
            case RegexProgram::AnyChar:
                printf("any char\n");
                break;
            case RegexProgram::Jump:
                printf("jump %zd\n", *reinterpret_cast<const RegexProgram::Offset*>(&insts[pos]));
                pos += sizeof(RegexProgram::Offset);
                break;
            case RegexProgram::Split:
            {
                int count = insts[pos++];
                printf("split [");
                for (int i = 0; i < count; ++i)
                    printf("%zd%s", reinterpret_cast<const RegexProgram::Offset*>(&insts[pos])[i],
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

bool regex_match(const RegexProgram& program, StringView data)
{
    const char* start = program.instructions.data();
    Vector<const char*> threads = { start };

    struct StepResult
    {
        enum Result { Consumed, Stepped, Matched, Failed } result;
        const char* next = nullptr;
    };
    auto step_thread = [&](const char* inst, char c) -> StepResult
    {
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
                return { StepResult::Stepped, start + *reinterpret_cast<const RegexProgram::Offset*>(inst) };
            case RegexProgram::Split:
            {
                const int count = *inst++;
                auto* offsets = reinterpret_cast<const RegexProgram::Offset*>(inst);
                for (int o = 1; o < count; ++o)
                    threads.push_back(start + offsets[o]);
                return { StepResult::Stepped, start + offsets[0] };
            }
            case RegexProgram::LineStart:
                // TODO
                return { StepResult::Stepped, inst };
            case RegexProgram::LineEnd:
                // TODO
                return { StepResult::Stepped, inst };
            case RegexProgram::Match:
                return { StepResult::Matched };
        }
        return { StepResult::Failed };
    };

    for (auto c : data)
    {
        for (int i = 0; i < threads.size(); ++i)
        {
            while (threads[i])
            {
                auto res = step_thread(threads[i], c);
                threads[i] = res.next;

                if (res.result == StepResult::Consumed or
                    res.result == StepResult::Failed)
                    break;
                else if (res.result == StepResult::Matched)
                    return true;
            }
        }
        threads.erase(std::remove(threads.begin(), threads.end(), nullptr), threads.end());
        if (threads.empty())
            break;
    }

    // Step remaining threads to see if they match without consuming anything else
    for (int i = 0; i < threads.size(); ++i)
    {
        while (threads[i])
        {
            auto res = step_thread(threads[i], 0);
            threads[i] = res.next;
            if (res.result == StepResult::Consumed)
                break;
            else if (res.result == StepResult::Matched)
                return true;
        }
    }
    return false;
}

auto test_regex = UnitTest{[]{
    StringView re = "^(foo|qux)+(bar)?baz$";
    auto node = RegexCompiler::Parser<const char*>::parse(re.begin(), re.end());
    kak_assert(node);
    auto program = RegexCompiler::compile(node);
    dump_program(program);
    kak_assert(regex_match(program, "fooquxbarbaz"));
    kak_assert(not regex_match(program, "quxbar"));
    kak_assert(not regex_match(program, "blahblah"));
    kak_assert(regex_match(program, "foobaz"));
}};

}
