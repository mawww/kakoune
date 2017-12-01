#ifndef regex_impl_hh_INCLUDED
#define regex_impl_hh_INCLUDED

#include "exception.hh"
#include "flags.hh"
#include "ref_ptr.hh"
#include "unicode.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "vector.hh"

namespace Kakoune
{

struct regex_error : runtime_error
{
    using runtime_error::runtime_error;
};

enum class MatchDirection
{
    Forward,
    Backward
};

enum class CharacterType : unsigned char
{
    None                    = 0,
    Whitespace              = 1 << 0,
    HorizontalWhitespace    = 1 << 1,
    Word                    = 1 << 2,
    Digit                   = 1 << 3,
    NotWhitespace           = 1 << 4,
    NotHorizontalWhitespace = 1 << 5,
    NotWord                 = 1 << 6,
    NotDigit                = 1 << 7
};
constexpr bool with_bit_ops(Meta::Type<CharacterType>) { return true; }

struct CharacterClass
{
    struct Range { Codepoint min, max; };

    Vector<Range, MemoryDomain::Regex> ranges;
    CharacterType ctypes = CharacterType::None;
    bool negative = false;
    bool ignore_case = false;
};

bool is_character_class(const CharacterClass& character_class, Codepoint cp);
bool is_ctype(CharacterType ctype, Codepoint cp);

struct CompiledRegex : RefCountable, UseMemoryDomain<MemoryDomain::Regex>
{
    enum Op : char
    {
        Match,
        FindNextStart,
        Literal,
        Literal_IgnoreCase,
        AnyChar,
        Class,
        CharacterType,
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
        LookAhead_IgnoreCase,
        NegativeLookAhead_IgnoreCase,
        LookBehind_IgnoreCase,
        NegativeLookBehind_IgnoreCase,
    };

    struct Instruction
    {
        Op op;
        // Those mutables are used during execution
        mutable bool scheduled;
        mutable uint16_t last_step;
        uint32_t param;
    };
    static_assert(sizeof(Instruction) == 8, "");

    static constexpr uint16_t search_prefix_size = 3;

    explicit operator bool() const { return not instructions.empty(); }

    Vector<Instruction, MemoryDomain::Regex> instructions;
    Vector<CharacterClass, MemoryDomain::Regex> character_classes;
    Vector<Codepoint, MemoryDomain::Regex> lookarounds;
    uint32_t first_backward_inst; // -1 if no backward support, 0 if only backward, >0 if both forward and backward
    uint32_t save_count;

    struct StartDesc
    {
        static constexpr size_t count = 256;
        static constexpr Codepoint other = 256;
        bool map[count+1];
    };

    std::unique_ptr<StartDesc> forward_start_desc;
    std::unique_ptr<StartDesc> backward_start_desc;
};

enum class RegexCompileFlags
{
    None     = 0,
    NoSubs   = 1 << 0,
    Optimize = 1 << 1,
    Backward = 1 << 1,
    NoForward = 1 << 2,
};
constexpr bool with_bit_ops(Meta::Type<RegexCompileFlags>) { return true; }

CompiledRegex compile_regex(StringView re, RegexCompileFlags flags);

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
    PrevAvailable     = 1 << 9,
};

constexpr bool with_bit_ops(Meta::Type<RegexExecFlags>) { return true; }

template<typename Iterator, MatchDirection direction>
class ThreadedRegexVM
{
public:
    ThreadedRegexVM(const CompiledRegex& program)
      : m_program{program}
    {
        kak_assert((direction == MatchDirection::Forward and program.first_backward_inst != 0) or
                   (direction == MatchDirection::Backward and program.first_backward_inst != -1));
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
            operator delete(saves);
        }
    }

    bool exec(Iterator begin, Iterator end, RegexExecFlags flags)
    {
        if (flags & RegexExecFlags::NotInitialNull and begin == end)
            return false;

        constexpr bool forward = direction == MatchDirection::Forward;
        const bool prev_avail = flags & RegexExecFlags::PrevAvailable;

        m_begin = Utf8It{utf8::iterator<Iterator>{forward ? begin : end,
                                                  prev_avail ? begin-1 : begin, end}};
        m_end = Utf8It{utf8::iterator<Iterator>{forward ? end : begin,
                                                prev_avail ? begin-1 : begin, end}};
        if (forward)
            m_flags = flags;
        else // Flip line begin/end flags as we flipped the instructions on compilation.
            m_flags = (RegexExecFlags)(flags & ~(RegexExecFlags::NotEndOfLine | RegexExecFlags::NotBeginOfLine)) |
                ((flags & RegexExecFlags::NotEndOfLine) ? RegexExecFlags::NotBeginOfLine : RegexExecFlags::None) |
                ((flags & RegexExecFlags::NotBeginOfLine) ? RegexExecFlags::NotEndOfLine : RegexExecFlags::None);

        const bool search = (flags & RegexExecFlags::Search);
        Utf8It start{m_begin};
        const auto& start_desc = direction == MatchDirection::Forward ? m_program.forward_start_desc
                                                                      : m_program.backward_start_desc;
        if (start_desc)
        {
            if (search)
            {
                to_next_start(start, m_end, *start_desc);
                if (start == m_end) // If start_desc is not null, it means we consume at least one char
                    return false;
            }
            else if (start != m_end and
                     not start_desc->map[std::min(*start, CompiledRegex::StartDesc::other)])
                return false;
        }

        ConstArrayView<CompiledRegex::Instruction> instructions{m_program.instructions};
        if (direction == MatchDirection::Forward)
            instructions = instructions.subrange(0, m_program.first_backward_inst);
        else
            instructions = instructions.subrange(m_program.first_backward_inst);
        if (not search)
            instructions = instructions.subrange(CompiledRegex::search_prefix_size);

        return exec_program(start, instructions);
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
        union // ref count when in use, next_free when in free list
        {
            int refcount;
            Saves* next_free;
        };
        Iterator pos[1];
    };

    template<bool copy>
    Saves* new_saves(Iterator* pos)
    {
        kak_assert(not copy or pos != nullptr);
        const auto count = m_program.save_count;
        if (m_first_free != nullptr)
        {
            Saves* res = m_first_free;
            m_first_free = res->next_free;
            res->refcount = 1;
            if (copy)
                std::copy(pos, pos + count, res->pos);
            else
                std::fill(res->pos, res->pos + count, Iterator{});

            return res;
        }

        void* ptr = operator new (sizeof(Saves) + (count-1) * sizeof(Iterator));
        Saves* saves = new (ptr) Saves{{1}, {copy ? pos[0] : Iterator{}}};
        for (size_t i = 1; i < count; ++i)
            new (&saves->pos[i]) Iterator{copy ? pos[i] : Iterator{}};
        m_saves.push_back(saves);
        return saves;
    }

    void release_saves(Saves* saves)
    {
        if (saves and --saves->refcount == 0)
        {
            saves->next_free = m_first_free;
            m_first_free = saves;
        }
    };

    struct Thread
    {
        const CompiledRegex::Instruction* inst;
        Saves* saves;
    };

    using Utf8It = std::conditional_t<direction == MatchDirection::Forward,
                                      utf8::iterator<Iterator>,
                                      std::reverse_iterator<utf8::iterator<Iterator>>>;

    struct ExecState
    {
        Vector<Thread, MemoryDomain::Regex> current_threads;
        Vector<Thread, MemoryDomain::Regex> next_threads;
        uint16_t step = -1;
    };

    enum class StepResult { Consumed, Matched, Failed, FindNextStart };

    // Steps a thread until it consumes the current character, matches or fail
    StepResult step(Utf8It& pos, Thread& thread, ExecState& state)
    {
        const bool no_saves = (m_flags & RegexExecFlags::NoSaves);
        auto* instructions = m_program.instructions.data();
        while (true)
        {
            auto& inst = *thread.inst++;
            if (inst.last_step == state.step)
                return StepResult::Failed;
            inst.last_step = state.step;

            switch (inst.op)
            {
                case CompiledRegex::Literal:
                    if (pos != m_end and inst.param == *pos)
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::Literal_IgnoreCase:
                    if (pos != m_end and inst.param == to_lower(*pos))
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::AnyChar:
                    return StepResult::Consumed;
                case CompiledRegex::Jump:
                    thread.inst = instructions + inst.param;
                    break;
                case CompiledRegex::Split_PrioritizeParent:
                {
                    if (thread.saves)
                        ++thread.saves->refcount;
                    state.current_threads.push_back({instructions + inst.param, thread.saves});
                    break;
                }
                case CompiledRegex::Split_PrioritizeChild:
                {
                    if (thread.saves)
                        ++thread.saves->refcount;
                    state.current_threads.push_back({thread.inst, thread.saves});
                    thread.inst = instructions + inst.param;
                    break;
                }
                case CompiledRegex::Save:
                {
                    if (no_saves)
                        break;
                    if (not thread.saves)
                        thread.saves = new_saves<false>(nullptr);
                    else if (thread.saves->refcount > 1)
                    {
                        --thread.saves->refcount;
                        thread.saves = new_saves<true>(thread.saves->pos);
                    }
                    thread.saves->pos[inst.param] = get_base(pos);
                    break;
                }
                case CompiledRegex::Class:
                    if (pos == m_end)
                        return StepResult::Failed;
                    return is_character_class(m_program.character_classes[inst.param], *pos) ?
                        StepResult::Consumed : StepResult::Failed;
                case CompiledRegex::CharacterType:
                    if (pos == m_end)
                        return StepResult::Failed;
                    return is_ctype((CharacterType)inst.param, *pos) ?
                        StepResult::Consumed : StepResult::Failed;;
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
                    if (lookaround<MatchDirection::Forward, false>(inst.param, pos) !=
                        (inst.op == CompiledRegex::LookAhead))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookAhead_IgnoreCase:
                case CompiledRegex::NegativeLookAhead_IgnoreCase:
                    if (lookaround<MatchDirection::Forward, true>(inst.param, pos) !=
                        (inst.op == CompiledRegex::LookAhead_IgnoreCase))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookBehind:
                case CompiledRegex::NegativeLookBehind:
                    if (lookaround<MatchDirection::Backward, false>(inst.param, pos) !=
                        (inst.op == CompiledRegex::LookBehind))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookBehind_IgnoreCase:
                case CompiledRegex::NegativeLookBehind_IgnoreCase:
                    if (lookaround<MatchDirection::Backward, true>(inst.param, pos) !=
                        (inst.op == CompiledRegex::LookBehind_IgnoreCase))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::FindNextStart:
                    kak_assert(state.current_threads.empty()); // search thread should by construction be the lower priority one
                    if (state.next_threads.empty())
                        return StepResult::FindNextStart;
                    return StepResult::Consumed;
                case CompiledRegex::Match:
                    return StepResult::Matched;
            }
        }
        return StepResult::Failed;
    }

    bool exec_program(Utf8It pos, ConstArrayView<CompiledRegex::Instruction> instructions)
    {
        ExecState state;
        state.current_threads.push_back({instructions.begin(), nullptr});

        const auto& start_desc = direction == MatchDirection::Forward ? m_program.forward_start_desc
                                                                      : m_program.backward_start_desc;

        bool found_match = false;
        while (true) // Iterate on all codepoints and once at the end
        {
            if (++state.step == 0)
            {
                // We wrapped, avoid potential collision on inst.last_step by resetting them
                for (auto& inst : instructions)
                    inst.last_step = 0;
                state.step = 1; // step 0 is never valid
            }

            bool find_next_start = false;
            while (not state.current_threads.empty())
            {
                auto thread = state.current_threads.back();
                state.current_threads.pop_back();
                switch (step(pos, thread, state))
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
                    found_match = true;

                    // remove this and lower priority threads
                    for (auto& t : state.current_threads)
                        release_saves(t.saves);
                    state.current_threads.clear();
                    break;
                case StepResult::Failed:
                    release_saves(thread.saves);
                    break;
                case StepResult::Consumed:
                    if (thread.inst->scheduled)
                    {
                        release_saves(thread.saves);
                        continue;
                    }
                    thread.inst->scheduled = true;
                    state.next_threads.push_back(thread);
                    break;
                case StepResult::FindNextStart:
                    state.next_threads.push_back(thread);
                    find_next_start = true;
                    break;
                }
            }
            for (auto& thread : state.next_threads)
                thread.inst->scheduled = false;

            if (pos == m_end or state.next_threads.empty() or
                (found_match and (m_flags & RegexExecFlags::AnyMatch)))
            {
                for (auto& t : state.next_threads)
                    release_saves(t.saves);
                return found_match;
            }

            std::swap(state.current_threads, state.next_threads);
            std::reverse(state.current_threads.begin(), state.current_threads.end());
            ++pos;

            if (find_next_start and start_desc)
                to_next_start(pos, m_end, *start_desc);
        }
    }

    void to_next_start(Utf8It& start, const Utf8It& end,
                       const CompiledRegex::StartDesc& start_desc)
    {
        while (start != end and *start >= 0 and
               not start_desc.map[std::min(*start, CompiledRegex::StartDesc::other)])
            ++start;
    }

    template<MatchDirection look_direction, bool ignore_case>
    bool lookaround(uint32_t index, Utf8It pos) const
    {
        for (auto it = m_program.lookarounds.begin() + index; *it != -1; ++it)
        {
            if (pos == (look_direction == MatchDirection::Forward ? m_end : m_begin))
                return false;
            Codepoint cp = (look_direction == MatchDirection::Forward ? *pos : *(pos-1));
            if (ignore_case)
                cp = to_lower(cp);

            const Codepoint ref = *it;
            if (ref == 0xF000)
            {} // any character matches
            else if (ref > 0xF0000 and ref < 0xF8000)
            {
                if (not is_character_class(m_program.character_classes[ref - 0xF0001], cp))
                    return false;
            }
            else if (ref >= 0xF8000 and ref <= 0xFFFFD)
            {
                if (not is_ctype((CharacterType)(ref & 0xFF), cp))
                    return false;
            }
            else if (ref != cp)
                return false;

            (look_direction == MatchDirection::Forward) ? ++pos : --pos;
        }
        return true;
    }

    bool is_line_start(const Utf8It& pos) const
    {
        if (not (m_flags & RegexExecFlags::PrevAvailable) and pos == m_begin)
            return not (m_flags & RegexExecFlags::NotBeginOfLine);
        return *(pos-1) == '\n';
    }

    bool is_line_end(const Utf8It& pos) const
    {
        if (pos == m_end)
            return not (m_flags & RegexExecFlags::NotEndOfLine);
        return *pos == '\n';
    }

    bool is_word_boundary(const Utf8It& pos) const
    {
        if (not (m_flags & RegexExecFlags::PrevAvailable) and pos == m_begin)
            return not (m_flags & RegexExecFlags::NotBeginOfWord);
        if (pos == m_end)
            return not (m_flags & RegexExecFlags::NotEndOfWord);
        return is_word(*(pos-1)) != is_word(*pos);
    }

    static const Iterator& get_base(const utf8::iterator<Iterator>& it) { return it.base(); }
    static Iterator get_base(const std::reverse_iterator<utf8::iterator<Iterator>>& it) { return it.base().base(); }

    const CompiledRegex& m_program;

    Utf8It m_begin;
    Utf8It m_end;
    RegexExecFlags m_flags;

    Vector<Saves*, MemoryDomain::Regex> m_saves;
    Saves* m_first_free = nullptr;

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
bool regex_match(It begin, It end, Vector<It, MemoryDomain::Regex>& captures, const CompiledRegex& re,
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
bool regex_search(It begin, It end, Vector<It, MemoryDomain::Regex>& captures, const CompiledRegex& re,
                  RegexExecFlags flags = RegexExecFlags::None)
{
    ThreadedRegexVM<It, direction> vm{re};
    if (vm.exec(begin, end, flags | RegexExecFlags::Search))
    {
        std::move(vm.captures().begin(), vm.captures().end(), std::back_inserter(captures));
        return true;
    }
    return false;
}

}

#endif // regex_impl_hh_INCLUDED
