#ifndef regex_impl_hh_INCLUDED
#define regex_impl_hh_INCLUDED

#include "exception.hh"
#include "flags.hh"
#include "unicode.hh"
#include "utf8.hh"
#include "vector.hh"
#include "utils.hh"
#include "unique_ptr.hh"

#include <bit>
#include <algorithm>

namespace Kakoune
{

struct regex_error : runtime_error
{
    using runtime_error::runtime_error;
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

bool is_ctype(CharacterType ctype, Codepoint cp);

struct CharacterClass
{
    struct Range
    {
        Codepoint min, max;
        friend bool operator==(const Range&, const Range&) = default;
    };

    Vector<Range, MemoryDomain::Regex> ranges;
    CharacterType ctypes = CharacterType::None;
    bool negative = false;
    bool ignore_case = false;

    friend bool operator==(const CharacterClass&, const CharacterClass&) = default;

    bool matches(Codepoint cp) const
    {
        if (ignore_case)
            cp = to_lower(cp);

        for (auto& [min, max] : ranges)
        {
            if (cp < min)
                break;
            else if (cp <= max)
                return not negative;
        }

        return (ctypes != CharacterType::None and is_ctype(ctypes, cp)) != negative;
    }

};

struct CompiledRegex : UseMemoryDomain<MemoryDomain::Regex>
{
    enum Op : char
    {
        Match,
        Literal,
        AnyChar,
        AnyCharExceptNewLine,
        CharRange,
        CharType,
        CharClass,
        Jump,
        Split,
        Save,
        LineAssertion,
        SubjectAssertion,
        WordBoundary,
        LookAround,
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

    union Param
    {
        struct Literal
        {
            uint32_t codepoint : 24;
            bool ignore_case : 1;
        } literal;
        struct CharRange
        {
            uint8_t min;
            uint8_t max;
            bool ignore_case : 1;
            bool negative;
        } range;
        CharacterType character_type;
        int16_t character_class_index;
        int16_t jump_offset;
        int16_t save_index;
        struct Split
        {
            int16_t offset;
            bool prioritize_parent : 1;
        } split;
        bool line_start;
        bool subject_begin;
        bool word_boundary_positive;
        struct Lookaround
        {
            int16_t index;
            bool ahead : 1;
            bool positive : 1;
            bool ignore_case : 1;
        } lookaround;
    };
    static_assert(sizeof(Param) == 4);

    struct Instruction
    {
        Op op;
        mutable uint16_t last_step; // mutable as used during execution
        Param param;
    };
#ifndef __ppc__
    static_assert(sizeof(Instruction) == 8);
#endif

    explicit operator bool() const { return not instructions.empty(); }

    struct NamedCapture
    {
        String name;
        uint32_t index;
    };

    Vector<Instruction, MemoryDomain::Regex> instructions;
    Vector<CharacterClass, MemoryDomain::Regex> character_classes;
    Vector<Lookaround, MemoryDomain::Regex> lookarounds;
    Vector<NamedCapture, MemoryDomain::Regex> named_captures;
    uint32_t first_backward_inst; // -1 if no backward support, 0 if only backward, >0 if both forward and backward
    uint32_t save_count;

    struct StartDesc : UseMemoryDomain<MemoryDomain::Regex>
    {
        static constexpr Codepoint count = 256;
        using OffsetLimits = std::numeric_limits<uint8_t>;
        char start_byte = 0;
        uint8_t offset = 0;
        bool map[count];
    };

    UniquePtr<StartDesc> forward_start_desc;
    UniquePtr<StartDesc> backward_start_desc;
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
    NotBeginOfLine    = 1 << 1,
    NotEndOfLine      = 1 << 2,
    NotBeginOfWord    = 1 << 3,
    NotEndOfWord      = 1 << 4,
    NotInitialNull    = 1 << 5,
};

constexpr bool with_bit_ops(Meta::Type<RegexExecFlags>) { return true; }

enum class RegexMode
{
    Forward  = 1 << 0,
    Backward = 1 << 1,
    Search   = 1 << 2,
    AnyMatch = 1 << 3,
    NoSaves  = 1 << 4,
};
constexpr bool with_bit_ops(Meta::Type<RegexMode>) { return true; }
constexpr bool has_direction(RegexMode mode)
{
    return (bool)(mode & RegexMode::Forward) xor
           (bool)(mode & RegexMode::Backward);
}

constexpr bool is_direction(RegexMode mode)
{
    return has_direction(mode) and
           (mode & ~(RegexMode::Forward | RegexMode::Backward)) == RegexMode{0};
}

template<typename It>
struct SentinelType { using Type = It; };

template<typename It>
    requires requires { typename It::Sentinel; }
struct SentinelType<It> { using Type = typename It::Sentinel; };

template<typename Iterator, RegexMode mode>
    requires (has_direction(mode))
class ThreadedRegexVM
{
public:
    ThreadedRegexVM(const CompiledRegex& program)
      : m_program{program}
    {
        kak_assert((forward and program.first_backward_inst != 0) or
                   (not forward and program.first_backward_inst != -1));
    }

    ThreadedRegexVM(ThreadedRegexVM&&) = default;
    ThreadedRegexVM& operator=(ThreadedRegexVM&&) = default;
    ThreadedRegexVM(const ThreadedRegexVM&) = delete;
    ThreadedRegexVM& operator=(const ThreadedRegexVM&) = delete;

    ~ThreadedRegexVM()
    {
        for (auto& saves : m_saves)
        {
            for (int i = m_program.save_count-1; i >= 0; --i)
                saves.pos[i].~Iterator();
            operator delete(saves.pos, m_program.save_count * sizeof(Iterator));
        }
    }

    bool exec(const Iterator& begin, const Iterator& end,
              const Iterator& subject_begin, const Iterator& subject_end,
              RegexExecFlags flags)
    {
        return exec(begin, end, subject_begin, subject_end, flags, []{});
    }

    bool exec(const Iterator& begin, const Iterator& end,
              const Iterator& subject_begin, const Iterator& subject_end,
              RegexExecFlags flags, auto&& idle_func)
    {
        if (flags & RegexExecFlags::NotInitialNull and begin == end)
            return false;

        const ExecConfig config{
            Sentinel{forward ? begin : end},
            Sentinel{forward ? end : begin},
            Sentinel{subject_begin},
            Sentinel{subject_end},
            flags
        };

        exec_program(forward ? begin : end, config, idle_func);

        while (not m_threads.next_is_empty())
            release_saves(m_threads.pop_next().saves);
        return m_found_match;
    }

    ArrayView<const Iterator> captures() const
    {
        if (m_captures >= 0)
        {
            auto& saves = m_saves[m_captures];
            for (int i = 0; i < m_program.save_count; ++i)
            {
                if ((saves.valid_mask & (1 << i)) == 0)
                    saves.pos[i] = Iterator{};
            }
            return { saves.pos, m_program.save_count };
        }
        return {};
    }

private:
    struct Saves
    {
        int32_t refcount;
        union {
            int32_t next_free;
            uint32_t valid_mask;
        };
        Iterator* pos;
    };

    template<bool copy>
    int16_t new_saves(Iterator* pos, uint32_t valid_mask)
    {
        kak_assert(not copy or pos != nullptr);
        const auto count = m_program.save_count;
        if (m_first_free >= 0)
        {
            const int16_t res = m_first_free;
            Saves& saves = m_saves[res];
            m_first_free = saves.next_free;
            kak_assert(saves.refcount == 1);
            if constexpr (copy)
                std::copy_n(pos, std::bit_width(valid_mask), saves.pos);
            saves.valid_mask = valid_mask;
            return res;
        }

        auto* new_pos = reinterpret_cast<Iterator*>(operator new (count * sizeof(Iterator)));
        for (size_t i = 0; i < count; ++i)
            new (new_pos+i) Iterator{copy ? pos[i] : Iterator{}};
        m_saves.push_back({1, {.valid_mask=valid_mask}, new_pos});
        return static_cast<int16_t>(m_saves.size() - 1);
    }

    void release_saves(int16_t index)
    {
        if (index < 0)
            return;
        auto& saves = m_saves[index];
        if (saves.refcount == 1)
        {
            saves.next_free = m_first_free;
            m_first_free = index;
        }
        else
            --saves.refcount;
    };

    struct Thread
    {
        const CompiledRegex::Instruction* inst;
        int saves;
    };

    using StartDesc = CompiledRegex::StartDesc;
    using Sentinel = typename SentinelType<Iterator>::Type;
    struct ExecConfig
    {
        const Sentinel begin;
        const Sentinel end;
        const Sentinel subject_begin;
        const Sentinel subject_end;
        const RegexExecFlags flags;
    };

    // Steps a thread until it consumes the current character, matches or fail
    [[gnu::always_inline]]
    void step_current_thread(const Iterator& pos, Codepoint cp, uint16_t current_step, const ExecConfig& config)
    {
        Thread thread = m_threads.pop_current();
        auto failed   = [&] { release_saves(thread.saves); };
        auto consumed = [&] { m_threads.push_next(thread); };

        while (true)
        {
            auto* inst = thread.inst++;
            auto [op, last_step, param] = *inst;
            // if this instruction was already executed for this step in another thread,
            // then this thread is redundant and can be dropped
            if (last_step == current_step)
                return failed();
            inst->last_step = current_step;

            switch (op)
            {
                case CompiledRegex::Match:
                    if ((pos != config.end and not (mode & RegexMode::Search)) or
                        (config.flags & RegexExecFlags::NotInitialNull and pos == config.begin))
                        return failed();

                    release_saves(m_captures);
                    m_captures = thread.saves;
                    m_found_match = true;

                    // remove lower priority threads
                    while (not m_threads.current_is_empty())
                        release_saves(m_threads.pop_current().saves);
                    return;
                case CompiledRegex::Literal:
                    if (pos != config.end and
                        param.literal.codepoint == (param.literal.ignore_case ? to_lower(cp) : cp))
                        return consumed();
                    return failed();
                case CompiledRegex::AnyChar:
                    return consumed();
                case CompiledRegex::AnyCharExceptNewLine:
                    if (pos != config.end and cp != '\n')
                        return consumed();
                    return failed();
                case CompiledRegex::CharRange:
                    if (auto actual_cp = (param.range.ignore_case ? to_lower(cp) : cp);
                        pos != config.end and
                        (actual_cp >= param.range.min and actual_cp <= param.range.max) != param.range.negative)
                        return consumed();
                    return failed();
                case CompiledRegex::CharType:
                    if (pos != config.end and is_ctype(param.character_type, cp))
                        return consumed();
                    return failed();
                case CompiledRegex::CharClass:
                    if (pos != config.end and
                        m_program.character_classes[param.character_class_index].matches(cp))
                        return consumed();
                    return failed();
                case CompiledRegex::Jump:
                    thread.inst = inst + param.jump_offset;
                    break;
                case CompiledRegex::Split:
                    if (auto* target = inst + param.split.offset;
                        target->last_step != current_step)
                    {
                        if (thread.saves >= 0)
                            ++m_saves[thread.saves].refcount;
                        if (not param.split.prioritize_parent)
                            std::swap(thread.inst, target);
                        m_threads.push_current({target, thread.saves});
                    }
                    break;
                case CompiledRegex::Save:
                    if constexpr (mode & RegexMode::NoSaves)
                        break;
                    if (thread.saves < 0)
                        thread.saves = new_saves<false>(nullptr, 0);
                    else if (auto& saves = m_saves[thread.saves]; saves.refcount > 1)
                    {
                        --saves.refcount;
                        thread.saves = new_saves<true>(saves.pos, saves.valid_mask);
                    }
                    m_saves[thread.saves].pos[param.save_index] = pos;
                    m_saves[thread.saves].valid_mask |= (1 << param.save_index);
                    break;
                case CompiledRegex::LineAssertion:
                    if (not (param.line_start ? is_line_start(pos, config) : is_line_end(pos, config)))
                        return failed();
                    break;
                case CompiledRegex::SubjectAssertion:
                    if (pos != (param.subject_begin ? config.subject_begin : config.subject_end))
                        return failed();
                    break;
                case CompiledRegex::WordBoundary:
                    if (is_word_boundary(pos, config) != param.word_boundary_positive)
                        return failed();
                    break;
                case CompiledRegex::LookAround:
                    if (lookaround(param.lookaround, pos, config) != param.lookaround.positive)
                        return failed();
                    break;
            }
        }
        return failed();
    }

    void exec_program(const Iterator& start, const ExecConfig& config, auto&& idle_func)
    {
        kak_assert(m_threads.current_is_empty() and m_threads.next_is_empty());
        m_threads.ensure_initial_capacity();
        release_saves(m_captures);
        m_captures = -1;
        m_found_match = false;

        const auto* start_desc = (forward ? m_program.forward_start_desc : m_program.backward_start_desc).get();
        Iterator next_start = start;
        if (start_desc)
        {
            if constexpr (mode & RegexMode::Search)
                next_start = find_next_start(start, config.end, *start_desc);
            if (next_start == config.end or // Non null start_desc means we consume at least one char
                (not (mode & RegexMode::Search) and
                  not start_desc->map[static_cast<unsigned char>(forward ? *start : *std::prev(start))]))
                return;
        }

        const auto insts = forward ? ArrayView(m_program.instructions).subrange(0, m_program.first_backward_inst)
                                   : ArrayView(m_program.instructions).subrange(m_program.first_backward_inst);
        m_threads.push_current({insts.begin(), -1});

        uint16_t current_step = -1;
        uint8_t idle_count = 0; // Run idle loop every 256 * 65536 == 16M codepoints
        Iterator pos = next_start;
        while (pos != config.end)
        {
            if (++current_step == 0)
            {
                if (++idle_count == 0)
                    idle_func();

                // We wrapped, avoid potential collision on inst.last_step by resetting them
                for (auto& inst : insts)
                    inst.last_step = 0;
                current_step = 1; // step 0 is never valid
            }

            auto next = pos;
            Codepoint cp = codepoint(next, config);

            while (not m_threads.current_is_empty())
                step_current_thread(pos, cp, current_step, config);

            if ((mode & RegexMode::Search) and not m_found_match)
            {
                if (start_desc)
                {
                    if (pos == next_start)
                        next_start = find_next_start(next, config.end, *start_desc);
                    if (m_threads.next_is_empty())
                        next = next_start;
                }
                if (not start_desc or next == next_start)
                    m_threads.push_next({insts.begin(), -1});
            }
            else if (m_threads.next_is_empty() or (m_found_match and (mode & RegexMode::AnyMatch)))
                return;

            pos = next;
            m_threads.swap_next();
        }

        if (++current_step == 0)
        {
            for (auto& inst : insts)
                inst.last_step = 0;
            current_step = 1; // step 0 is never valid
        }
        while (not m_threads.current_is_empty())
            step_current_thread(pos, -1, current_step, config);
    }

    static Iterator find_next_start(const Iterator& start, const Sentinel& end, const StartDesc& start_desc)
    {
        auto pos = start;
        if (char start_byte = start_desc.start_byte)
        {
            while (pos != end)
            {
                if constexpr (forward)
                {
                    if (*pos == start_byte)
                       return utf8::advance(pos, start, -CharCount(start_desc.offset));
                    ++pos;
                }
                else
                {
                    auto prev = utf8::previous(pos, end);
                    if (*prev == start_byte)
                       return utf8::advance(pos, start, CharCount(start_desc.offset));
                    pos = prev;
                }
            }
        }

        while (pos != end)
        {
            static_assert(StartDesc::count <= 256, "start desc should be ascii only");
            if constexpr (forward)
            {
                if (start_desc.map[static_cast<unsigned char>(*pos)])
                    return utf8::advance(pos, start, -CharCount(start_desc.offset));
                ++pos;
            }
            else
            {
                auto prev = utf8::previous(pos, end);
                if (start_desc.map[static_cast<unsigned char>(*prev)])
                    return utf8::advance(pos, start, CharCount(start_desc.offset));
                pos = prev;
            }
        }
        return pos;
    }

    bool lookaround(CompiledRegex::Param::Lookaround param, Iterator pos, const ExecConfig& config) const
    {
        using Lookaround = CompiledRegex::Lookaround;

        if (not param.ahead)
        {
            if (pos == config.subject_begin)
                return m_program.lookarounds[param.index] == Lookaround::EndOfLookaround;
            utf8::to_previous(pos, config.subject_begin);
        }

        for (auto it = m_program.lookarounds.begin() + param.index; *it != Lookaround::EndOfLookaround; ++it)
        {
            if (param.ahead and pos == config.subject_end)
                return false;

            Codepoint cp = utf8::codepoint(pos, config.subject_end);
            if (param.ignore_case)
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
                if (not m_program.character_classes[index].matches(cp))
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

            if (not param.ahead and pos == config.subject_begin)
                return *++it == Lookaround::EndOfLookaround;

            param.ahead ? utf8::to_next(pos, config.subject_end)
                        : utf8::to_previous(pos, config.subject_begin);
        }
        return true;
    }

    static bool is_line_start(const Iterator& pos, const ExecConfig& config)
    {
        if (pos == config.subject_begin)
            return not (config.flags & RegexExecFlags::NotBeginOfLine);
        return *(pos-1) == '\n';
    }

    static bool is_line_end(const Iterator& pos, const ExecConfig& config)
    {
        if (pos == config.subject_end)
            return not (config.flags & RegexExecFlags::NotEndOfLine);
        return *pos == '\n';
    }

    static bool is_word_boundary(const Iterator& pos, const ExecConfig& config)
    {
        if (pos == config.subject_begin)
            return not (config.flags & RegexExecFlags::NotBeginOfWord);
        if (pos == config.subject_end)
            return not (config.flags & RegexExecFlags::NotEndOfWord);
        return is_word(utf8::codepoint(utf8::previous(pos, config.subject_begin), config.subject_end)) !=
               is_word(utf8::codepoint(pos, config.subject_end));
    }

    [[gnu::flatten]]
    static Codepoint codepoint(Iterator& it, const ExecConfig& config)
    {
        if constexpr (forward)
        {
            return utf8::read_codepoint(it, config.end);
        }
        else
        {
            utf8::to_previous(it, config.end);
            return utf8::codepoint(it, config.begin);
        }
    }

    struct DualThreadStack
    {
        bool current_is_empty() const { return m_current == m_next_begin; }
        bool next_is_empty() const { return m_next_end == m_next_begin; }

        [[gnu::always_inline]]
        void push_current(Thread thread) { m_data[decrement(m_current)] = thread; grow_ifn(true); }
        [[gnu::always_inline]]
        Thread pop_current() { return m_data[post_increment(m_current)]; }

        [[gnu::always_inline]]
        void push_next(Thread thread) { m_data[post_increment(m_next_end)] = thread; grow_ifn(false); }
        [[gnu::always_inline]]
        Thread pop_next() { return m_data[decrement(m_next_end)]; }

        void swap_next()
        {
            m_current = m_next_begin;
            m_next_begin = m_next_end;
        }

        void ensure_initial_capacity()
        {
            if (m_capacity_mask != 0)
                return;

            constexpr uint32_t initial_capacity = 64 / sizeof(Thread);
            static_assert(initial_capacity >= 4 and std::has_single_bit(initial_capacity));
            m_data.reset(new Thread[initial_capacity]);
            m_capacity_mask = initial_capacity-1;
        }

        [[gnu::always_inline]]
        void grow_ifn(bool pushed_current)
        {
            if (m_current == m_next_end)
                grow(pushed_current);
        }

        [[gnu::noinline]]
        void grow(bool pushed_current)
        {
            auto capacity = m_capacity_mask + 1;
            const auto new_capacity = capacity * 2;
            Thread* new_data = new Thread[new_capacity];
            Thread* old_data = m_data.get();
            std::rotate_copy(old_data, old_data + m_current, old_data + capacity, new_data);
            m_next_begin = (m_next_begin - m_current) & m_capacity_mask;
            if (pushed_current and m_next_begin == 0)
                m_next_begin = capacity;
            m_next_end = capacity;
            m_current = 0;

            m_data.reset(new_data);
            kak_assert(std::has_single_bit(new_capacity));
            m_capacity_mask = new_capacity-1;
        }

    private:
        uint32_t decrement(uint32_t& index)
        {
            index = (index - 1) & m_capacity_mask;
            return index;
        }

        uint32_t post_increment(uint32_t& index)
        {
            auto res = index;
            index = (index + 1) & m_capacity_mask;
            return res;
        }

        UniquePtr<Thread[]> m_data;
        uint32_t m_capacity_mask = 0; // Maximum capacity should be 2*instruction count, so 65536
        uint32_t m_current = 0;
        uint32_t m_next_begin = 0;
        uint32_t m_next_end = 0;
    };

    static constexpr bool forward = mode & RegexMode::Forward;

    const CompiledRegex& m_program;
    DualThreadStack m_threads;
    Vector<Saves, MemoryDomain::Regex> m_saves;
    int16_t m_first_free = -1;
    int16_t m_captures = -1;
    bool m_found_match = false;
};

}

#endif // regex_impl_hh_INCLUDED
