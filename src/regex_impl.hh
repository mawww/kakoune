#ifndef regex_impl_hh_INCLUDED
#define regex_impl_hh_INCLUDED

#include "unicode.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "vector.hh"
#include "flags.hh"

namespace Kakoune
{

struct CompiledRegex
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

    using Offset = unsigned;
    explicit operator bool() const { return not bytecode.empty(); }

    Vector<char> bytecode;
    Vector<std::function<bool (Codepoint)>> matchers;
    size_t save_count;
};

CompiledRegex compile_regex(StringView re);

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

template<typename Iterator>
struct ThreadedRegexVM
{
    ThreadedRegexVM(const CompiledRegex& program)
      : m_program{program} { kak_assert(m_program); }

    ThreadedRegexVM(const ThreadedRegexVM&) = delete;

    ~ThreadedRegexVM()
    {
        for (auto* saves : m_saves)
        {
            for (size_t i = m_program.save_count-1; i > 0; --i)
                saves->pos[i].~Iterator();
            saves->~Saves();
        }
    }

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
        const char* inst;
        Saves* saves;
    };

    using Utf8It = utf8::iterator<Iterator>;

    enum class StepResult { Consumed, Matched, Failed };
    StepResult step(const Utf8It& pos, Thread& thread, Vector<Thread>& threads)
    {
        const auto prog_start = m_program.bytecode.data();
        const auto prog_end = prog_start + m_program.bytecode.size();
        while (true)
        {
            const Codepoint cp = pos == m_end ? 0 : *pos;
            const CompiledRegex::Op op = (CompiledRegex::Op)*thread.inst++;
            switch (op)
            {
                case CompiledRegex::Literal:
                    if (utf8::read_codepoint(thread.inst, prog_end) == cp)
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::LiteralIgnoreCase:
                    if (utf8::read_codepoint(thread.inst, prog_end) == to_lower(cp))
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::AnyChar:
                    return StepResult::Consumed;
                case CompiledRegex::Jump:
                    thread.inst = prog_start + *reinterpret_cast<const CompiledRegex::Offset*>(thread.inst);
                    break;
                case CompiledRegex::Split_PrioritizeParent:
                {
                    auto parent = thread.inst + sizeof(CompiledRegex::Offset);
                    auto child = prog_start + *reinterpret_cast<const CompiledRegex::Offset*>(thread.inst);
                    thread.inst = parent;
                    if (thread.saves)
                        ++thread.saves->refcount;
                    threads.push_back({child, thread.saves});
                    break;
                }
                case CompiledRegex::Split_PrioritizeChild:
                {
                    auto parent = thread.inst + sizeof(CompiledRegex::Offset);
                    auto child = prog_start + *reinterpret_cast<const CompiledRegex::Offset*>(thread.inst);
                    thread.inst = child;
                    if (thread.saves)
                        ++thread.saves->refcount;
                    threads.push_back({parent, thread.saves});
                    break;
                }
                case CompiledRegex::Save:
                {
                    const size_t index = *thread.inst++;
                    if (thread.saves == nullptr)
                        break;
                    if (thread.saves->refcount > 1)
                    {
                        --thread.saves->refcount;
                        thread.saves = new_saves<true>(thread.saves->pos);
                    }
                    thread.saves->pos[index] = pos.base();
                    break;
                }
                case CompiledRegex::Matcher:
                {
                    const int matcher_id = *thread.inst++;
                    return m_program.matchers[matcher_id](cp) ?
                        StepResult::Consumed : StepResult::Failed;
                }
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
                    int count = *thread.inst++;
                    for (auto it = pos; count and it != m_end; ++it, --count)
                        if (*it != utf8::read(thread.inst))
                            break;
                    if ((op == CompiledRegex::LookAhead and count != 0) or
                        (op == CompiledRegex::NegativeLookAhead and count == 0))
                        return StepResult::Failed;
                    thread.inst = utf8::advance(thread.inst, prog_end, CharCount{count - 1});
                    break;
                }
                case CompiledRegex::LookBehind:
                case CompiledRegex::NegativeLookBehind:
                {
                    int count = *thread.inst++;
                    for (auto it = pos-1; count and it >= m_begin; --it, --count)
                        if (*it != utf8::read(thread.inst))
                            break;
                    if ((op == CompiledRegex::LookBehind and count != 0) or
                        (op == CompiledRegex::NegativeLookBehind and count == 0))
                        return StepResult::Failed;
                    thread.inst = utf8::advance(thread.inst, prog_end, CharCount{count - 1});
                    break;
                }
                case CompiledRegex::Match:
                    return StepResult::Matched;
            }
        }
        return StepResult::Failed;
    }

    bool exec_from(const Utf8It& start, Saves* initial_saves, Vector<Thread>& current_threads, Vector<Thread>& next_threads)
    {
        current_threads.push_back({m_program.bytecode.data(), initial_saves});
        next_threads.clear();

        bool found_match = false;
        for (Utf8It pos = start; pos != m_end; ++pos)
        {
            while (not current_threads.empty())
            {
                auto thread = current_threads.back();
                current_threads.pop_back();
                switch (step(pos, thread, current_threads))
                {
                case StepResult::Matched:
                    if (not (m_flags & RegexExecFlags::Search) or // We are not at end, this is not a full match
                        (m_flags & RegexExecFlags::NotInitialNull and pos == m_begin))
                    {
                        release_saves(thread.saves);
                        continue;
                    }

                    release_saves(m_captures);
                    m_captures = thread.saves;
                    if (m_flags & RegexExecFlags::AnyMatch)
                        return true;

                    found_match = true;
                    current_threads.clear(); // remove this and lower priority threads
                    break;
                case StepResult::Failed:
                    release_saves(thread.saves);
                    break;
                case StepResult::Consumed:
                    if (contains_that(next_threads, [&](auto& t) { return t.inst == thread.inst; }))
                        release_saves(thread.saves);
                    else
                        next_threads.push_back(thread);
                    break;
                }
            }
            if (next_threads.empty())
                return found_match;

            std::swap(current_threads, next_threads);
            std::reverse(current_threads.begin(), current_threads.end());
        }
        if (found_match)
            return true;

        // Step remaining threads to see if they match without consuming anything else
        const Utf8It end{m_end, m_begin, m_end};
        while (not current_threads.empty())
        {
            auto thread = current_threads.back();
            current_threads.pop_back();
            if (step(end, thread, current_threads) == StepResult::Matched)
            {
                release_saves(m_captures);
                m_captures = thread.saves;
                return true;
            }
        }
        return false;
    }

    bool exec(Iterator begin, Iterator end, RegexExecFlags flags)
    {
        m_begin = begin;
        m_end = end;
        m_flags = flags;

        if (flags & RegexExecFlags::NotInitialNull and m_begin == m_end)
            return false;

        Vector<Thread> current_threads, next_threads;

        const bool no_saves = (m_flags & RegexExecFlags::NoSaves);
        Utf8It start{m_begin, m_begin, m_end};
        if (exec_from(start, no_saves ? nullptr : new_saves<false>(nullptr),
                      current_threads, next_threads))
            return true;

        if (not (flags & RegexExecFlags::Search))
            return false;

        while (start != end)
        {
            if (exec_from(++start, no_saves ? nullptr : new_saves<false>(nullptr),
                          current_threads, next_threads))
                return true;
        }
        return false;
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

    const CompiledRegex& m_program;

    Iterator m_begin;
    Iterator m_end;
    RegexExecFlags m_flags;

    Vector<Saves*> m_saves;
    Vector<Saves*> m_free_saves;

    Saves* m_captures = nullptr;
};

template<typename It>
bool regex_match(It begin, It end, const CompiledRegex& re, RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It> vm{re};
    return vm.exec(begin, end, (RegexExecFlags)(flags & ~(RegexExecFlags::Search)) |
                               RegexExecFlags::AnyMatch | RegexExecFlags::NoSaves);
}

template<typename It>
bool regex_match(It begin, It end, Vector<It>& captures, const CompiledRegex& re,
                 RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It> vm{re};
    if (vm.exec(begin, end,  flags & ~(RegexExecFlags::Search)))
    {
        std::copy(vm.m_captures->pos, vm.m_captures->pos + re.save_count, std::back_inserter(captures));
        return true;
    }
    return false;
}

template<typename It>
bool regex_search(It begin, It end, const CompiledRegex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It> vm{re};
    return vm.exec(begin, end, flags | RegexExecFlags::Search | RegexExecFlags::AnyMatch | RegexExecFlags::NoSaves);
}

template<typename It>
bool regex_search(It begin, It end, Vector<It>& captures, const CompiledRegex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It> vm{re};
    if (vm.exec(begin, end, flags | RegexExecFlags::Search))
    {
        std::copy(vm.m_captures->pos, vm.m_captures->pos + re.save_count, std::back_inserter(captures));
        return true;
    }
    return false;
}

}

#endif // regex_impl_hh_INCLUDED
