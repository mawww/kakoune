#ifndef regex_impl_hh_INCLUDED
#define regex_impl_hh_INCLUDED

#include "exception.hh"
#include "flags.hh"
#include "ref_ptr.hh"
#include "unicode.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "vector.hh"

#include <string.h>

namespace Kakoune
{

enum class MatchDirection
{
    Forward,
    Backward
};

struct CompiledRegex : RefCountable
{
    enum Op : char
    {
        Match,
        Literal,
        LiteralIgnoreCase,
        AnyChar,
        Matcher,
        Jump,
        Split_PrioritizeParent,
        Split_PrioritizeChild,
        Save,
        LineStart,
        LineEnd,
        WordBoundary,
        NotWordBoundary,
        SubjectBegin,
        SubjectEnd,
        LookAhead,
        NegativeLookAhead,
        LookBehind,
        NegativeLookBehind,
    };

    struct Instruction
    {
        Op op;
        mutable bool processed;
        mutable bool scheduled;
        uint32_t param;
    };
    static_assert(sizeof(Instruction) == 8, "");

    explicit operator bool() const { return not instructions.empty(); }

    Vector<Instruction> instructions;
    Vector<std::function<bool (Codepoint)>> matchers;
    Vector<Codepoint> lookarounds;
    MatchDirection direction;
    size_t save_count;

    struct StartChars { bool map[256]; };
    std::unique_ptr<StartChars> start_chars;
};

CompiledRegex compile_regex(StringView re, MatchDirection direction = MatchDirection::Forward);

enum class RegexExecFlags
{
    None              = 0,
    Search            = 1 << 0,
    NotBeginOfLine    = 1 << 1,
    NotEndOfLine      = 1 << 2,
    NotBeginOfWord    = 1 << 3,
    NotEndOfWord      = 1 << 4,
    NotBeginOfSubject = 1 << 5,
    NotInitialNull    = 1 << 6,
    AnyMatch          = 1 << 7,
    NoSaves           = 1 << 8,
};

constexpr bool with_bit_ops(Meta::Type<RegexExecFlags>) { return true; }

template<typename Iterator, MatchDirection direction>
struct ChooseUtf8It
{
    using Type = utf8::iterator<Iterator>;
};

template<typename Iterator>
struct ChooseUtf8It<Iterator, MatchDirection::Backward>
{
    using Type = std::reverse_iterator<utf8::iterator<Iterator>>;
};

template<typename Iterator, MatchDirection direction>
class ThreadedRegexVM
{
public:
    ThreadedRegexVM(const CompiledRegex& program)
      : m_program{program}
      {
          kak_assert(m_program);
          if (direction != program.direction)
              throw runtime_error{"Regex and VM direction mismatch"};
      }

    ThreadedRegexVM(const ThreadedRegexVM&) = delete;
    ThreadedRegexVM& operator=(const ThreadedRegexVM&) = delete;

    ~ThreadedRegexVM()
    {
        for (auto* saves : m_saves)
        {
            for (size_t i = m_program.save_count-1; i > 0; --i)
                saves->pos[i].~Iterator();
            saves->~Saves();
        }
    }

    bool exec(Iterator begin, Iterator end, RegexExecFlags flags)
    {
        const bool forward = direction == MatchDirection::Forward;
        m_begin = Utf8It{utf8::iterator<Iterator>{forward ? begin : end, begin, end}};
        m_end = Utf8It{utf8::iterator<Iterator>{forward ? end : begin, begin, end}};
        m_flags = flags;

        if (flags & RegexExecFlags::NotInitialNull and m_begin == m_end)
            return false;

        Vector<Thread> current_threads, next_threads;

        const bool no_saves = (m_flags & RegexExecFlags::NoSaves);
        Utf8It start{m_begin};

        const bool* start_chars = m_program.start_chars ? m_program.start_chars->map : nullptr;

        if (flags & RegexExecFlags::Search)
            to_next_start(start, m_end, start_chars);

        if (exec_from(start, no_saves ? nullptr : new_saves<false>(nullptr),
                      current_threads, next_threads))
            return true;

        if (not (flags & RegexExecFlags::Search))
            return false;

        do
        {
            to_next_start(++start, m_end, start_chars);
            if (exec_from(start, no_saves ? nullptr : new_saves<false>(nullptr),
                          current_threads, next_threads))
                return true;
        }
        while (start != m_end);

        return false;
    }

    ArrayView<const Iterator> captures() const
    {
        if (m_captures)
            return { m_captures->pos, m_program.save_count };
        return {};
    }

private:
    struct Saves
    {
        int refcount;
        Iterator pos[1];
    };

    template<bool copy>
    Saves* new_saves(Iterator* pos)
    {
        kak_assert(not copy or pos != nullptr);
        const auto count = m_program.save_count;
        if (not m_free_saves.empty())
        {
            Saves* res = m_free_saves.back();
            m_free_saves.pop_back();
            res->refcount = 1;
            if (copy)
                std::copy(pos, pos + count, res->pos);
            else
                std::fill(res->pos, res->pos + count, Iterator{});

            return res;
        }

        void* ptr = ::operator new (sizeof(Saves) + (count-1) * sizeof(Iterator));
        Saves* saves = new (ptr) Saves{1, copy ? pos[0] : Iterator{}};
        for (size_t i = 1; i < count; ++i)
            new (&saves->pos[i]) Iterator{copy ? pos[i] : Iterator{}};
        m_saves.push_back(saves);
        return saves;
    }

    void release_saves(Saves* saves)
    {
        if (saves and --saves->refcount == 0)
            m_free_saves.push_back(saves);
    };

    struct Thread
    {
        uint32_t inst;
        Saves* saves;
    };

    using Utf8It = typename ChooseUtf8It<Iterator, direction>::Type;

    enum class StepResult { Consumed, Matched, Failed };

    // Steps a thread until it consumes the current character, matches or fail
    StepResult step(const Utf8It& pos, Thread& thread, Vector<Thread>& threads)
    {
        while (true)
        {
            auto& inst = m_program.instructions[thread.inst++];
            if (inst.processed)
                return StepResult::Failed;
            inst.processed = true;

            switch (inst.op)
            {
                case CompiledRegex::Literal:
                    if (pos != m_end and inst.param == *pos)
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::LiteralIgnoreCase:
                    if (pos != m_end and inst.param == to_lower(*pos))
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::AnyChar:
                    return StepResult::Consumed;
                case CompiledRegex::Jump:
                    thread.inst = inst.param;
                    break;
                case CompiledRegex::Split_PrioritizeParent:
                {
                    if (thread.saves)
                        ++thread.saves->refcount;
                    threads.push_back({inst.param, thread.saves});
                    break;
                }
                case CompiledRegex::Split_PrioritizeChild:
                {
                    if (thread.saves)
                        ++thread.saves->refcount;
                    threads.push_back({thread.inst, thread.saves});
                    thread.inst = inst.param;
                    break;
                }
                case CompiledRegex::Save:
                {
                    if (thread.saves == nullptr)
                        break;
                    if (thread.saves->refcount > 1)
                    {
                        --thread.saves->refcount;
                        thread.saves = new_saves<true>(thread.saves->pos);
                    }
                    thread.saves->pos[inst.param] = get_base(pos);
                    break;
                }
                case CompiledRegex::Matcher:
                    if (pos == m_end)
                        return StepResult::Failed;
                    return m_program.matchers[inst.param](*pos) ?
                        StepResult::Consumed : StepResult::Failed;
                case CompiledRegex::LineStart:
                    if (not is_line_start(pos))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LineEnd:
                    if (not is_line_end(pos))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::WordBoundary:
                    if (not is_word_boundary(pos))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::NotWordBoundary:
                    if (is_word_boundary(pos))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::SubjectBegin:
                    if (pos != m_begin or (m_flags & RegexExecFlags::NotBeginOfSubject))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::SubjectEnd:
                    if (pos != m_end)
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookAhead:
                case CompiledRegex::NegativeLookAhead:
                {
                    auto ref = m_program.lookarounds.begin() + inst.param;
                    for (auto it = pos; *ref != -1 and it != m_end; ++it, ++ref)
                        if (*it != *ref)
                            break;
                    if ((inst.op == CompiledRegex::LookAhead and *ref != -1) or
                        (inst.op == CompiledRegex::NegativeLookAhead and *ref == -1))
                        return StepResult::Failed;
                    break;
                }
                case CompiledRegex::LookBehind:
                case CompiledRegex::NegativeLookBehind:
                {
                    auto ref = m_program.lookarounds.begin() + inst.param;
                    for (auto it = pos-1; *ref != -1 and it >= m_begin; --it, ++ref)
                        if (*it != *ref)
                            break;
                    if ((inst.op == CompiledRegex::LookBehind and *ref != -1) or
                        (inst.op == CompiledRegex::NegativeLookBehind and *ref == -1))
                        return StepResult::Failed;
                    break;
                }
                case CompiledRegex::Match:
                    return StepResult::Matched;
            }
        }
        return StepResult::Failed;
    }

    bool exec_from(Utf8It pos, Saves* initial_saves, Vector<Thread>& current_threads, Vector<Thread>& next_threads)
    {
        current_threads.push_back({0, initial_saves});
        next_threads.clear();

        bool found_match = false;
        while (true) // Iterate on all codepoints and once at the end
        {
            for (auto& inst : m_program.instructions)
            {
                inst.processed = false;
                inst.scheduled = false;
            }

            while (not current_threads.empty())
            {
                auto thread = current_threads.back();
                current_threads.pop_back();
                switch (step(pos, thread, current_threads))
                {
                case StepResult::Matched:
                    if ((pos != m_end and not (m_flags & RegexExecFlags::Search)) or
                        (m_flags & RegexExecFlags::NotInitialNull and pos == m_begin))
                    {
                        release_saves(thread.saves);
                        continue;
                    }

                    release_saves(m_captures);
                    m_captures = thread.saves;
                    if (pos == m_end or (m_flags & RegexExecFlags::AnyMatch))
                        return true;

                    found_match = true;
                    current_threads.clear(); // remove this and lower priority threads
                    break;
                case StepResult::Failed:
                    release_saves(thread.saves);
                    break;
                case StepResult::Consumed:
                    if (m_program.instructions[thread.inst].scheduled)
                    {
                        release_saves(thread.saves);
                        continue;
                    }
                    m_program.instructions[thread.inst].scheduled = true;
                    next_threads.push_back(thread);
                    break;
                }
            }
            if (pos == m_end or next_threads.empty())
                return found_match;

            std::swap(current_threads, next_threads);
            std::reverse(current_threads.begin(), current_threads.end());
            ++pos;
        }
    }

    void to_next_start(Utf8It& start, const Utf8It& end, const bool* start_chars)
    {
        if (not start_chars)
            return;

        while (start != end and *start >= 0 and *start < 256 and
               not start_chars[*start])
            ++start;
    }

    bool is_line_start(const Utf8It& pos) const
    {
        return (pos == m_begin and not (m_flags & RegexExecFlags::NotBeginOfLine)) or
               *(pos-1) == '\n';
    }

    bool is_line_end(const Utf8It& pos) const
    {
        return (pos == m_end and not (m_flags & RegexExecFlags::NotEndOfLine)) or
               *pos == '\n';
    }

    bool is_word_boundary(const Utf8It& pos) const
    {
        return (pos == m_begin and not (m_flags & RegexExecFlags::NotBeginOfWord)) or
               (pos == m_end and not (m_flags & RegexExecFlags::NotEndOfWord)) or
               is_word(*(pos-1)) != is_word(*pos);
    }

    static const Iterator& get_base(const utf8::iterator<Iterator>& it) { return it.base(); }
    static Iterator get_base(const std::reverse_iterator<utf8::iterator<Iterator>>& it) { return it.base().base(); }

    const CompiledRegex& m_program;

    Utf8It m_begin;
    Utf8It m_end;
    RegexExecFlags m_flags;

    Vector<Saves*> m_saves;
    Vector<Saves*> m_free_saves;

    Saves* m_captures = nullptr;
};

template<typename It, MatchDirection direction = MatchDirection::Forward>
bool regex_match(It begin, It end, const CompiledRegex& re, RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It, direction> vm{re};
    return vm.exec(begin, end, (RegexExecFlags)(flags & ~(RegexExecFlags::Search)) |
                               RegexExecFlags::AnyMatch | RegexExecFlags::NoSaves);
}

template<typename It, MatchDirection direction = MatchDirection::Forward>
bool regex_match(It begin, It end, Vector<It>& captures, const CompiledRegex& re,
                 RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It, direction> vm{re};
    if (vm.exec(begin, end,  flags & ~(RegexExecFlags::Search)))
    {
        std::copy(vm.captures().begin(), vm.captures().end(), std::back_inserter(captures));
        return true;
    }
    return false;
}

template<typename It, MatchDirection direction = MatchDirection::Forward>
bool regex_search(It begin, It end, const CompiledRegex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It, direction> vm{re};
    return vm.exec(begin, end, flags | RegexExecFlags::Search | RegexExecFlags::AnyMatch | RegexExecFlags::NoSaves);
}

template<typename It, MatchDirection direction = MatchDirection::Forward>
bool regex_search(It begin, It end, Vector<It>& captures, const CompiledRegex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It, direction> vm{re};
    if (vm.exec(begin, end, flags | RegexExecFlags::Search))
    {
        std::copy(vm.captures().begin(), vm.captures().end(), std::back_inserter(captures));
        return true;
    }
    return false;
}

}

#endif // regex_impl_hh_INCLUDED
