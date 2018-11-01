#ifndef regex_impl_hh_INCLUDED
#define regex_impl_hh_INCLUDED

#include "exception.hh"
#include "flags.hh"
#include "ref_ptr.hh"
#include "unicode.hh"
#include "utf8.hh"
#include "utf8_iterator.hh"
#include "vector.hh"
#include "utils.hh"

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
        AnyCharExceptNewLine,
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

    enum class Lookaround : Codepoint
    {
        OpBegin              = 0xF0000,
        AnyChar              = 0xF0000,
        AnyCharExceptNewLine = 0xF0001,
        CharacterClass       = 0xF0002,
        CharacterType        = 0xF8000,
        OpEnd                = 0xFFFFF,
        EndOfLookaround      = static_cast<Codepoint>(-1)
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
    Vector<Lookaround, MemoryDomain::Regex> lookarounds;
    uint32_t first_backward_inst; // -1 if no backward support, 0 if only backward, >0 if both forward and backward
    uint32_t save_count;

    struct StartDesc : UseMemoryDomain<MemoryDomain::Regex>
    {
        static constexpr Codepoint count = 128;
        static constexpr Codepoint other = 0;
        bool map[count];
    };

    std::unique_ptr<StartDesc> forward_start_desc;
    std::unique_ptr<StartDesc> backward_start_desc;
};

String dump_regex(const CompiledRegex& program);

enum class RegexCompileFlags
{
    None     = 0,
    NoSubs   = 1 << 0,
    Optimize = 1 << 1,
    Backward = 1 << 2,
    NoForward = 1 << 3,
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
    NotInitialNull    = 1 << 5,
    AnyMatch          = 1 << 6,
    NoSaves           = 1 << 7,
};

constexpr bool with_bit_ops(Meta::Type<RegexExecFlags>) { return true; }

template<typename It, typename=void>
struct SentinelType { using Type = It; };

template<typename It>
struct SentinelType<It, void_t<typename It::Sentinel>> { using Type = typename It::Sentinel; };

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

    bool exec(Iterator begin, Iterator end,
              Iterator subject_begin, Iterator subject_end,
              RegexExecFlags flags)
    {
        if (flags & RegexExecFlags::NotInitialNull and begin == end)
            return false;

        constexpr bool forward = direction == MatchDirection::Forward;


        if (not forward) // Flip line begin/end flags as we flipped the instructions on compilation.
            flags = (RegexExecFlags)(flags & ~(RegexExecFlags::NotEndOfLine | RegexExecFlags::NotBeginOfLine)) |
                ((flags & RegexExecFlags::NotEndOfLine) ? RegexExecFlags::NotBeginOfLine : RegexExecFlags::None) |
                ((flags & RegexExecFlags::NotBeginOfLine) ? RegexExecFlags::NotEndOfLine : RegexExecFlags::None);

        const bool search = (flags & RegexExecFlags::Search);

        ConstArrayView<CompiledRegex::Instruction> instructions{m_program.instructions};
        if (direction == MatchDirection::Forward)
            instructions = instructions.subrange(0, m_program.first_backward_inst);
        else
            instructions = instructions.subrange(m_program.first_backward_inst);
        if (not search)
            instructions = instructions.subrange(CompiledRegex::search_prefix_size);


        const ExecConfig config{
            Sentinel{forward ? begin : end},
            Sentinel{forward ? end : begin},
            Sentinel{forward ? subject_begin : subject_end},
            Sentinel{forward ? subject_end : subject_begin},
            flags,
            instructions
        };

        EffectiveIt start{Utf8It{
            forward ? begin : end,
            Sentinel{subject_begin},
            Sentinel{subject_end}
        }};

        if (const auto& start_desc = direction == MatchDirection::Forward ?
            m_program.forward_start_desc : m_program.backward_start_desc)
        {
            if (search)
            {
                to_next_start(start, config.end, *start_desc);
                if (base(start) == config.end) // If start_desc is not null, it means we consume at least one char
                    return false;
            }
            else if (base(start) != config.end and
                     not start_desc->map[*start < StartDesc::count ? *start : StartDesc::other])
                return false;
        }

        return exec_program(std::move(start), config);
    }

    ArrayView<const Iterator> captures() const
    {
        if (m_captures >= 0)
            return { m_saves[m_captures]->pos, m_program.save_count };
        return {};
    }

private:
    struct Saves
    {
        union // ref count when in use, next_free when in free list
        {
            int16_t refcount;
            int16_t next_free;
        };
        Iterator pos[1];
    };

    template<bool copy>
    int16_t new_saves(Iterator* pos)
    {
        kak_assert(not copy or pos != nullptr);
        const auto count = m_program.save_count;
        if (m_first_free >= 0)
        {
            const int16_t res = m_first_free;
            Saves* save = m_saves[res];
            m_first_free = save->next_free;
            save->refcount = 1;
            if (copy)
                std::copy(pos, pos + count, save->pos);
            else
                std::fill(save->pos, save->pos + count, Iterator{});

            return res;
        }

        void* ptr = operator new (sizeof(Saves) + (count-1) * sizeof(Iterator));
        Saves* saves = new (ptr) Saves{{1}, {copy ? pos[0] : Iterator{}}};
        for (size_t i = 1; i < count; ++i)
            new (&saves->pos[i]) Iterator{copy ? pos[i] : Iterator{}};
        m_saves.push_back(saves);
        return static_cast<int16_t>(m_saves.size() - 1);
    }

    void release_saves(int16_t saves)
    {
        if (saves >= 0 and --m_saves[saves]->refcount == 0)
        {
            m_saves[saves]->next_free = m_first_free;
            m_first_free = saves;
        }
    };

    struct Thread
    {
        int16_t inst;
        int16_t saves;
    };

    using StartDesc = CompiledRegex::StartDesc;
    using Sentinel = typename SentinelType<Iterator>::Type;
    using Utf8It = utf8::iterator<Iterator, Sentinel>;
    using EffectiveIt = std::conditional_t<direction == MatchDirection::Forward,
                                           Utf8It, std::reverse_iterator<Utf8It>>;

    struct ExecConfig
    {
        const Sentinel begin;
        const Sentinel end;
        const Sentinel subject_begin;
        const Sentinel subject_end;
        const RegexExecFlags flags;
        ConstArrayView<CompiledRegex::Instruction> instructions;
    };

    enum class StepResult { Consumed, Matched, Failed, FindNextStart };

    // Steps a thread until it consumes the current character, matches or fail
    StepResult step(EffectiveIt& pos, uint16_t current_step, Thread& thread, const ExecConfig& config)
    {
        const bool no_saves = (config.flags & RegexExecFlags::NoSaves);
        auto* instructions = m_program.instructions.data();
        while (true)
        {
            auto& inst = instructions[thread.inst++];
            // if this instruction was already executed for this step in another thread,
            // then this thread is redundant and can be dropped
            if (inst.last_step == current_step)
                return StepResult::Failed;
            inst.last_step = current_step;

            switch (inst.op)
            {
                case CompiledRegex::Literal:
                    if (base(pos) != config.end and inst.param == *pos)
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::Literal_IgnoreCase:
                    if (base(pos) != config.end and inst.param == to_lower(*pos))
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::AnyChar:
                    return StepResult::Consumed;
                case CompiledRegex::AnyCharExceptNewLine:
                    if (base(pos) != config.end and *pos != '\n')
                        return StepResult::Consumed;
                    return StepResult::Failed;
                case CompiledRegex::Jump:
                    thread.inst = static_cast<int16_t>(inst.param);
                    break;
                case CompiledRegex::Split_PrioritizeParent:
                {
                    if (thread.saves >= 0)
                        ++m_saves[thread.saves]->refcount;
                    m_threads.push_current({static_cast<int16_t>(inst.param), thread.saves});
                    break;
                }
                case CompiledRegex::Split_PrioritizeChild:
                {
                    if (thread.saves >= 0)
                        ++m_saves[thread.saves]->refcount;
                    m_threads.push_current({thread.inst, thread.saves});
                    thread.inst = static_cast<uint16_t>(inst.param);
                    break;
                }
                case CompiledRegex::Save:
                {
                    if (no_saves)
                        break;
                    if (thread.saves < 0)
                        thread.saves = new_saves<false>(nullptr);
                    else if (m_saves[thread.saves]->refcount > 1)
                    {
                        --m_saves[thread.saves]->refcount;
                        thread.saves = new_saves<true>(m_saves[thread.saves]->pos);
                    }
                    m_saves[thread.saves]->pos[inst.param] = base(pos);
                    break;
                }
                case CompiledRegex::Class:
                    if (base(pos) == config.end)
                        return StepResult::Failed;
                    return is_character_class(m_program.character_classes[inst.param], *pos) ?
                        StepResult::Consumed : StepResult::Failed;
                case CompiledRegex::CharacterType:
                    if (base(pos) == config.end)
                        return StepResult::Failed;
                    return is_ctype((CharacterType)inst.param, *pos) ?
                        StepResult::Consumed : StepResult::Failed;
                case CompiledRegex::LineStart:
                    if (not is_line_start(pos, config))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LineEnd:
                    if (not is_line_end(pos, config))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::WordBoundary:
                    if (not is_word_boundary(pos, config))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::NotWordBoundary:
                    if (is_word_boundary(pos, config))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::SubjectBegin:
                    if (base(pos) != config.subject_begin)
                        return StepResult::Failed;
                    break;
                case CompiledRegex::SubjectEnd:
                    if (base(pos) != config.subject_end)
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookAhead:
                case CompiledRegex::NegativeLookAhead:
                    if (lookaround<MatchDirection::Forward, false>(inst.param, pos, config) !=
                        (inst.op == CompiledRegex::LookAhead))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookAhead_IgnoreCase:
                case CompiledRegex::NegativeLookAhead_IgnoreCase:
                    if (lookaround<MatchDirection::Forward, true>(inst.param, pos, config) !=
                        (inst.op == CompiledRegex::LookAhead_IgnoreCase))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookBehind:
                case CompiledRegex::NegativeLookBehind:
                    if (lookaround<MatchDirection::Backward, false>(inst.param, pos, config) !=
                        (inst.op == CompiledRegex::LookBehind))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::LookBehind_IgnoreCase:
                case CompiledRegex::NegativeLookBehind_IgnoreCase:
                    if (lookaround<MatchDirection::Backward, true>(inst.param, pos, config) !=
                        (inst.op == CompiledRegex::LookBehind_IgnoreCase))
                        return StepResult::Failed;
                    break;
                case CompiledRegex::FindNextStart:
                    kak_assert(m_threads.current_is_empty()); // search thread should by construction be the lower priority one
                    if (m_threads.next_is_empty())
                        return StepResult::FindNextStart;
                    return StepResult::Consumed;
                case CompiledRegex::Match:
                    return StepResult::Matched;
            }
        }
        return StepResult::Failed;
    }

    bool exec_program(EffectiveIt pos, const ExecConfig& config)
    {
        kak_assert(m_threads.current_is_empty() and m_threads.next_is_empty());
        release_saves(m_captures);
        m_captures = -1;
        m_threads.push_current({static_cast<int16_t>(&config.instructions[0] - &m_program.instructions[0]), -1});

        const auto& start_desc = direction == MatchDirection::Forward ? m_program.forward_start_desc
                                                                      : m_program.backward_start_desc;

        uint16_t current_step = -1;
        bool found_match = false;
        while (true) // Iterate on all codepoints and once at the end
        {
            if (++current_step == 0)
            {
                // We wrapped, avoid potential collision on inst.last_step by resetting them
                for (auto& inst : config.instructions)
                    inst.last_step = 0;
                current_step = 1; // step 0 is never valid
            }

            bool find_next_start = false;
            while (not m_threads.current_is_empty())
            {
                auto thread = m_threads.pop_current();
                switch (step(pos, current_step, thread, config))
                {
                case StepResult::Matched:
                    if ((base(pos) != config.end and not (config.flags & RegexExecFlags::Search)) or
                        (config.flags & RegexExecFlags::NotInitialNull and base(pos) == config.begin))
                    {
                        release_saves(thread.saves);
                        continue;
                    }

                    release_saves(m_captures);
                    m_captures = thread.saves;
                    found_match = true;

                    // remove this and lower priority threads
                    while (not m_threads.current_is_empty())
                        release_saves(m_threads.pop_current().saves);
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
                    m_threads.push_next(thread);
                    break;
                case StepResult::FindNextStart:
                    m_threads.push_next(thread);
                    find_next_start = true;
                    break;
                }
            }
            for (auto& thread : m_threads.next_threads())
                m_program.instructions[thread.inst].scheduled = false;

            if (base(pos) == config.end or m_threads.next_is_empty() or
                (found_match and (config.flags & RegexExecFlags::AnyMatch)))
            {
                for (auto& t : m_threads.next_threads())
                    release_saves(t.saves);
                m_threads.clear_next();
                return found_match;
            }

            m_threads.swap_next();
            ++pos;

            if (find_next_start and start_desc)
                to_next_start(pos, config.end, *start_desc);
        }
    }

    void to_next_start(EffectiveIt& start, const Sentinel& end, const StartDesc& start_desc)
    {
        while (base(start) != end)
        {
            const Codepoint cp = read(start);
            if (start_desc.map[(cp >= 0 and cp < StartDesc::count) ? cp : StartDesc::other])
            {
                --start;
                return;
            }
        }
    }

    template<MatchDirection look_direction, bool ignore_case>
    bool lookaround(uint32_t index, EffectiveIt pos, const ExecConfig& config) const
    {
        using Lookaround = CompiledRegex::Lookaround;

        const auto end = (look_direction == MatchDirection::Forward ? config.subject_end : config.subject_begin);
        for (auto it = m_program.lookarounds.begin() + index; *it != Lookaround::EndOfLookaround; ++it)
        {
            if (base(pos) == end)
                return false;
            Codepoint cp = (look_direction == MatchDirection::Forward ? *pos : *(pos-1));
            if (ignore_case)
                cp = to_lower(cp);

            const Lookaround op = *it;
            if (op == Lookaround::AnyChar)
            {} // any character matches
            else if (op == Lookaround::AnyCharExceptNewLine)
            {
                if (cp == '\n')
                    return false;
            }
            else if (op >= Lookaround::CharacterClass and op < Lookaround::CharacterType)
            {
                auto index = to_underlying(op) - to_underlying(Lookaround::CharacterClass);
                if (not is_character_class(m_program.character_classes[index], cp))
                    return false;
            }
            else if (op >= Lookaround::CharacterType and op < Lookaround::OpEnd)
            {
                auto ctype = static_cast<CharacterType>(to_underlying(op) & 0xFF);
                if (not is_ctype(ctype, cp))
                    return false;
            }
            else if (static_cast<Codepoint>(op) != cp)
                return false;

            (look_direction == MatchDirection::Forward) ? ++pos : --pos;
        }
        return true;
    }

    static bool is_line_start(const EffectiveIt& pos, const ExecConfig& config)
    {
        if (base(pos) == config.subject_begin)
            return not (config.flags & RegexExecFlags::NotBeginOfLine);
        return *(pos-1) == '\n';
    }

    static bool is_line_end(const EffectiveIt& pos, const ExecConfig& config)
    {
        if (base(pos) == config.subject_end)
            return not (config.flags & RegexExecFlags::NotEndOfLine);
        return *pos == '\n';
    }

    static bool is_word_boundary(const EffectiveIt& pos, const ExecConfig& config)
    {
        if (base(pos) == config.subject_begin)
            return not (config.flags & RegexExecFlags::NotBeginOfWord);
        if (base(pos) == config.subject_end)
            return not (config.flags & RegexExecFlags::NotEndOfWord);
        return is_word(*(pos-1)) != is_word(*pos);
    }

    static Codepoint read(Utf8It& it) { return it.read(); }
    static Codepoint read(std::reverse_iterator<Utf8It>& it) { Codepoint cp = *it; ++it; return cp; }

    static const Iterator& base(const Utf8It& it) { return it.base(); }
    static Iterator base(const std::reverse_iterator<Utf8It>& it) { return it.base().base(); }

    const CompiledRegex& m_program;

    struct DualThreadStack
    {
        bool current_is_empty() const { return m_current == 0; }
        bool next_is_empty() const { return m_next == m_capacity; }

        void push_current(Thread thread) { grow_ifn(); m_data[m_current++] = thread; }
        Thread pop_current() { kak_assert(m_current > 0); return m_data[--m_current]; }

        void push_next(Thread thread) { grow_ifn(); m_data[--m_next] = thread; }
        void clear_next() { m_next = m_capacity; }
        ConstArrayView<Thread> next_threads() const { return { m_data.get() + m_next, m_data.get() + m_capacity }; }

        void swap_next()
        {
            for (; m_next < m_capacity; m_current++, m_next++)
                m_data[m_current] = m_data[m_next];
        }

    private:
        void grow_ifn()
        {
            if (m_current != m_next)
                return;
            const auto new_capacity = m_capacity ? m_capacity * 2 : 4;
            Thread* new_data = new Thread[new_capacity];
            Thread* data = m_data.get();
            std::copy(data, data + m_current, new_data);
            const auto new_next = new_capacity - (m_capacity - m_next);
            std::copy(data + m_next, data + m_capacity, new_data + new_next);
            m_capacity = new_capacity;
            m_next = new_next;
            m_data.reset(new_data);
        }

        std::unique_ptr<Thread[]> m_data = nullptr;
        int32_t m_capacity = 0; // Maximum capacity should be 2*instruction count, so 65536
        int32_t m_current = 0;
        int32_t m_next = 0;
    };

    DualThreadStack m_threads;
    Vector<Saves*, MemoryDomain::Regex> m_saves;
    int16_t m_first_free = -1;
    int16_t m_captures = -1;
};

}

#endif // regex_impl_hh_INCLUDED
