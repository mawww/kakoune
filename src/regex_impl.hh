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

    using Offset = unsigned;
    explicit operator bool() const { return not bytecode.empty(); }

    Vector<char> bytecode;
    Vector<std::function<bool (Codepoint)>> matchers;
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
        std::unique_ptr<bool[]> inst_processed{new bool[m_program.bytecode.size()]};

        const bool no_saves = (m_flags & RegexExecFlags::NoSaves);
        Utf8It start{m_begin};

        const bool* start_chars = m_program.start_chars ? m_program.start_chars->map : nullptr;

        if (flags & RegexExecFlags::Search)
            to_next_start(start, m_end, start_chars);

        if (exec_from(start, no_saves ? nullptr : new_saves<false>(nullptr),
                      current_threads, next_threads, inst_processed.get()))
            return true;

        if (not (flags & RegexExecFlags::Search))
            return false;

        do
        {
            to_next_start(++start, m_end, start_chars);
            if (exec_from(start, no_saves ? nullptr : new_saves<false>(nullptr),
                          current_threads, next_threads, inst_processed.get()))
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
        const char* inst;
        Saves* saves;
    };

    using Utf8It = typename ChooseUtf8It<Iterator, direction>::Type;

    enum class StepResult { Consumed, Matched, Failed };

    // Steps a thread until it consumes the current character, matches or fail
    StepResult step(const Utf8It& pos, Thread& thread, Vector<Thread>& threads, bool* inst_processed)
    {
        const auto prog_start = m_program.bytecode.data();
        const auto prog_end = prog_start + m_program.bytecode.size();
        while (true)
        {
            // If we have hit this instruction on this character, in this thread or another, do not try again
            const auto inst_offset = thread.inst - prog_start;
            if (inst_processed[inst_offset])
                return StepResult::Failed;
            inst_processed[inst_offset] = true;

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
                    thread.inst = prog_start + get_offset(thread.inst);
                    break;
                case CompiledRegex::Split_PrioritizeParent:
                {
                    auto parent = thread.inst + sizeof(CompiledRegex::Offset);
                    auto child = prog_start + get_offset(thread.inst);
                    thread.inst = parent;
                    if (thread.saves)
                        ++thread.saves->refcount;
                    threads.push_back({child, thread.saves});
                    break;
                }
                case CompiledRegex::Split_PrioritizeChild:
                {
                    auto parent = thread.inst + sizeof(CompiledRegex::Offset);
                    auto child = prog_start + get_offset(thread.inst);
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
                    thread.saves->pos[index] = get_base(pos);
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

    bool exec_from(const Utf8It& start, Saves* initial_saves, Vector<Thread>& current_threads, Vector<Thread>& next_threads, bool* inst_processed)
    {
        current_threads.push_back({m_program.bytecode.data(), initial_saves});
        next_threads.clear();

        bool found_match = false;
        for (Utf8It pos = start; pos != m_end; ++pos)
        {
            memset(inst_processed, 0, m_program.bytecode.size() * sizeof(bool));
            while (not current_threads.empty())
            {
                auto thread = current_threads.back();
                current_threads.pop_back();
                switch (step(pos, thread, current_threads, inst_processed))
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

        memset(inst_processed, 0, m_program.bytecode.size() * sizeof(bool));
        // Step remaining threads to see if they match without consuming anything else
        while (not current_threads.empty())
        {
            auto thread = current_threads.back();
            current_threads.pop_back();
            if (step(m_end, thread, current_threads, inst_processed) == StepResult::Matched)
            {
                release_saves(m_captures);
                m_captures = thread.saves;
                return true;
            }
        }
        return false;
    }

    void to_next_start(Utf8It& start, const Utf8It& end, const bool* start_chars)
    {
        if (not start_chars)
            return;

        while (start != end and *start >= 0 and *start < 256 and
               not start_chars[*start])
            ++start;
    }

    static CompiledRegex::Offset get_offset(const char* ptr)
    {
        CompiledRegex::Offset res;
        memcpy(&res, ptr, sizeof(CompiledRegex::Offset));
        return res;
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
