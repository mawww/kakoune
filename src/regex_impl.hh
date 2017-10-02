#ifndef regex_impl_hh_INCLUDED
#define regex_impl_hh_INCLUDED

#include "unicode.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "vector.hh"

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
        LookBehind,
        NegativeLookAhead,
        NegativeLookBehind,
    };

    using Offset = unsigned;
    static constexpr Offset search_prefix_size = 3 + 2 * sizeof(Offset);

    explicit operator bool() const { return not bytecode.empty(); }

    Vector<char> bytecode;
    Vector<std::function<bool (Codepoint)>> matchers;
    size_t save_count;
};

CompiledRegex compile_regex(StringView re);

template<typename Iterator>
struct ThreadedRegexVM
{
    ThreadedRegexVM(const CompiledRegex& program)
      : m_program{program} { kak_assert(m_program); }

    struct Thread
    {
        const char* inst;
        Vector<Iterator> saves = {};
    };

    enum class StepResult { Consumed, Matched, Failed };
    StepResult step(size_t thread_index)
    {
        const auto prog_start = m_program.bytecode.data();
        const auto prog_end = prog_start + m_program.bytecode.size();
        while (true)
        {
            auto& thread = m_threads[thread_index];
            const Codepoint cp = m_pos == m_end ? 0 : *m_pos;
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
                {
                    auto inst = prog_start + *reinterpret_cast<const CompiledRegex::Offset*>(thread.inst);
                    // if instruction is already going to be executed by another thread, drop this thread
                    if (std::find_if(m_threads.begin(), m_threads.end(),
                                     [inst](const Thread& t) { return t.inst == inst; }) != m_threads.end())
                        return StepResult::Failed;
                    thread.inst = inst;
                    break;
                }
                case CompiledRegex::Split_PrioritizeParent:
                {
                    auto new_thread_inst = prog_start + *reinterpret_cast<const CompiledRegex::Offset*>(thread.inst);
                    thread.inst += sizeof(CompiledRegex::Offset);
                    add_thread(thread_index+1, new_thread_inst, thread.saves);
                    break;
                }
                case CompiledRegex::Split_PrioritizeChild:
                {
                    auto new_thread_inst = thread.inst + sizeof(CompiledRegex::Offset);
                    thread.inst = prog_start + *reinterpret_cast<const CompiledRegex::Offset*>(thread.inst);
                    add_thread(thread_index+1, new_thread_inst, thread.saves);
                    break;
                }
                case CompiledRegex::Save:
                {
                    const char index = *thread.inst++;
                    thread.saves[index] = m_pos.base();
                    break;
                }
                case CompiledRegex::Matcher:
                {
                    const int matcher_id = *thread.inst++;
                    return m_program.matchers[matcher_id](*m_pos) ?
                        StepResult::Consumed : StepResult::Failed;
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
                    if (m_pos != m_begin)
                        return StepResult::Failed;
                    break;
                case CompiledRegex::SubjectEnd:
                    if (m_pos != m_end)
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookAhead:
                case CompiledRegex::NegativeLookAhead:
                {
                    int count = *thread.inst++;
                    for (auto it = m_pos; count and it != m_end; ++it, --count)
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
                    for (auto it = m_pos-1; count and it >= m_begin; --it, --count)
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

    bool exec(Iterator begin, Iterator end, bool match = true, bool longest = false)
    {
        bool found_match = false;
        m_threads.clear();
        const auto start_offset = (match ? CompiledRegex::search_prefix_size : 0);
        add_thread(0, m_program.bytecode.data() + start_offset,
                   Vector<Iterator>(m_program.save_count, Iterator{}));

        m_begin = begin;
        m_end = end;

        for (m_pos = Utf8It{m_begin, m_begin, m_end}; m_pos != m_end; ++m_pos)
        {
            for (int i = 0; i < m_threads.size(); )
            {
                const auto res = step(i);
                if (res == StepResult::Matched)
                {
                    if (match)
                    {
                        m_threads.erase(m_threads.begin() + i);
                        continue; // We are not at end, this is not a full match
                    }

                    m_captures = std::move(m_threads[i].saves);
                    found_match = true;
                    m_threads.resize(i); // remove this and lower priority threads
                    if (not longest)
                        return true;
                }
                else if (res == StepResult::Failed)
                    m_threads.erase(m_threads.begin() + i);
                else
                {
                    auto it = m_threads.begin() + i;
                    if (std::find_if(m_threads.begin(), it, [inst = it->inst](auto& t)
                                     { return t.inst == inst; }) != it)
                        m_threads.erase(it);
                    else
                        ++i;
                }
            }
            // we should never have more than one thread on the same instruction
            kak_assert(m_threads.size() <= m_program.bytecode.size());
            if (m_threads.empty())
                return found_match;
        }

        // Step remaining threads to see if they match without consuming anything else
        for (int i = 0; i < m_threads.size(); ++i)
        {
            if (step(i) == StepResult::Matched)
            {
                m_captures = std::move(m_threads[i].saves);
                found_match = true;
                m_threads.resize(i); // remove this and lower priority threads
                if (not longest)
                    return true;
            }
        }
        return found_match;
    }

    void add_thread(int index, const char* inst, Vector<Iterator> saves)
    {
        if (std::find_if(m_threads.begin(), m_threads.end(),
                         [inst](const Thread& t) { return t.inst == inst; }) == m_threads.end())
            m_threads.insert(m_threads.begin() + index, {inst, std::move(saves)});
        kak_assert(m_threads.size() < m_program.bytecode.size());
    }

    bool is_line_start() const
    {
        return m_pos == m_begin or *(m_pos-1) == '\n';
    }

    bool is_line_end() const
    {
        return m_pos == m_end or *m_pos == '\n';
    }

    bool is_word_boundary() const
    {
        return m_pos == m_begin or m_pos == m_end or
               is_word(*(m_pos-1)) != is_word(*m_pos);
    }

    const CompiledRegex& m_program;
    Vector<Thread> m_threads;

    using Utf8It = utf8::iterator<Iterator>;

    Iterator m_begin;
    Iterator m_end;
    Utf8It m_pos;

    Vector<Iterator> m_captures;
};

template<typename It>
bool regex_match(It begin, It end, const CompiledRegex& re)
{
    ThreadedRegexVM<It> vm{re};
    return vm.exec(begin, end, true, false);
}

template<typename It>
bool regex_match(It begin, It end, Vector<It>& captures, const CompiledRegex& re)
{
    ThreadedRegexVM<It> vm{re};
    if (vm.exec(begin, end, true, true))
    {
        captures = std::move(vm.m_captures);
        return true;
    }
    return false;
}

template<typename It>
bool regex_search(It begin, It end, const CompiledRegex& re)
{
    ThreadedRegexVM<It> vm{re};
    return vm.exec(begin, end, false, false);
}

template<typename It>
bool regex_search(It begin, It end, Vector<It>& captures, const CompiledRegex& re)
{
    ThreadedRegexVM<It> vm{re};
    if (vm.exec(begin, end, false, true))
    {
        captures = std::move(vm.m_captures);
        return true;
    }
    return false;
}

}

#endif // regex_impl_hh_INCLUDED
