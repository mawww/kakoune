#ifndef regex_impl_hh_INCLUDED
#define regex_impl_hh_INCLUDED

#include "exception.hh"
#include "flags.hh"
#include "ref_ptr.hh"
#include "unicode.hh"
#include "utf8.hh"
#include "vector.hh"
#include "utils.hh"

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

        for (auto& range : ranges)
        {
            if (cp < range.min)
                break;
            else if (cp <= range.max)
                return not negative;
        }

        return (ctypes != CharacterType::None and is_ctype(ctypes, cp)) != negative;
    }

};

struct CompiledRegex : RefCountable, UseMemoryDomain<MemoryDomain::Regex>
{
    enum Op : char
    {
        Match,
        Literal,
        AnyChar,
        AnyCharExceptNewLine,
        CharClass,
        CharType,
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
        int16_t character_class_index;
        CharacterType character_type;
        int16_t jump_target;
        int16_t save_index;
        struct Split
        {
            int16_t target;
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
    static_assert(sizeof(Instruction) == 8);

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

template<typename It, typename=void>
struct SentinelType { using Type = It; };

template<typename It>
struct SentinelType<It, void_t<typename It::Sentinel>> { using Type = typename It::Sentinel; };

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
        for (auto* saves : m_saves)
        {
            for (size_t i = m_program.save_count-1; i > 0; --i)
                saves->pos[i].~Iterator();
            saves->~Saves();
            operator delete(saves);
        }
    }

    bool exec(const Iterator& begin, const Iterator& end,
              const Iterator& subject_begin, const Iterator& subject_end,
              RegexExecFlags flags)
    {
        if (flags & RegexExecFlags::NotInitialNull and begin == end)
            return false;

        constexpr bool search = (mode & RegexMode::Search);

        const ExecConfig config{
            Sentinel{forward ? begin : end},
            Sentinel{forward ? end : begin},
            Sentinel{subject_begin},
            Sentinel{subject_end},
            flags
        };

        Iterator start = forward ? begin : end;
        if (const auto& start_desc = forward ? m_program.forward_start_desc : m_program.backward_start_desc)
        {
            if (search)
            {
                to_next_start(start, config, *start_desc);
                if (start == config.end) // If start_desc is not null, it means we consume at least one char
                    return false;
            }
            else if (start != config.end)
            {
                const unsigned char c = forward ? *start : *utf8::previous(start, config.end);
                if (not start_desc->map[(c < StartDesc::count) ? c : StartDesc::other])
                    return false;
            }
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
        int16_t refcount;
        int16_t next_free;
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
            Saves& saves = *m_saves[res];
            m_first_free = saves.next_free;
            kak_assert(saves.refcount == 1);
            if (copy)
                std::copy_n(pos, count, saves.pos);
            else
                std::fill_n(saves.pos, count, Iterator{});

            return res;
        }

        void* ptr = operator new (sizeof(Saves) + (count-1) * sizeof(Iterator));
        Saves* saves = new (ptr) Saves{1, 0, {copy ? pos[0] : Iterator{}}};
        for (size_t i = 1; i < count; ++i)
            new (&saves->pos[i]) Iterator{copy ? pos[i] : Iterator{}};
        m_saves.push_back(saves);
        return static_cast<int16_t>(m_saves.size() - 1);
    }

    void release_saves(int16_t index)
    {
        if (index < 0)
            return;
        auto& saves = *m_saves[index];
        if (saves.refcount == 1)
        {
            saves.next_free = m_first_free;
            m_first_free = index;
        }
        else
            --saves.refcount;
    };

    struct alignas(int32_t) Thread
    {
        int16_t inst;
        int16_t saves;
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
    void step_thread(const Iterator& pos, Codepoint cp, uint16_t current_step, Thread thread, const ExecConfig& config)
    {
        auto failed = [this, &thread]() {
            release_saves(thread.saves);
        };
        auto consumed = [this, &thread]() {
            m_threads.push_next(thread);
        };

        auto* instructions = m_program.instructions.data();
        while (true)
        {
            auto& inst = instructions[thread.inst++];
            // if this instruction was already executed for this step in another thread,
            // then this thread is redundant and can be dropped
            if (inst.last_step == current_step)
                return failed();
            inst.last_step = current_step;

            switch (inst.op)
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
                        inst.param.literal.codepoint == (inst.param.literal.ignore_case ? to_lower(cp) : cp))
                        return consumed();
                    return failed();
                case CompiledRegex::AnyChar:
                    return consumed();
                case CompiledRegex::AnyCharExceptNewLine:
                    if (pos != config.end and cp != '\n')
                        return consumed();
                    return failed();
                case CompiledRegex::Jump:
                    thread.inst = inst.param.jump_target;
                    break;
                case CompiledRegex::Split:
                    if (thread.saves >= 0)
                        ++m_saves[thread.saves]->refcount;

                    if (inst.param.split.prioritize_parent)
                        m_threads.push_current({inst.param.split.target, thread.saves});
                    else
                    {
                        m_threads.push_current(thread);
                        thread.inst = inst.param.split.target;
                    }
                    break;
                case CompiledRegex::Save:
                    if (mode & RegexMode::NoSaves)
                        break;
                    if (thread.saves < 0)
                        thread.saves = new_saves<false>(nullptr);
                    else if (m_saves[thread.saves]->refcount > 1)
                    {
                        --m_saves[thread.saves]->refcount;
                        thread.saves = new_saves<true>(m_saves[thread.saves]->pos);
                    }
                    m_saves[thread.saves]->pos[inst.param.save_index] = pos;
                    break;
                case CompiledRegex::CharClass:
                    if (pos == config.end)
                        return failed();
                    return m_program.character_classes[inst.param.character_class_index].matches(cp) ? consumed() : failed();
                case CompiledRegex::CharType:
                    if (pos == config.end)
                        return failed();
                    return is_ctype(inst.param.character_type, cp) ? consumed() : failed();
                case CompiledRegex::LineAssertion:
                    if (not (inst.param.line_start ? is_line_start(pos, config) : is_line_end(pos, config)))
                        return failed();
                    break;
                case CompiledRegex::SubjectAssertion:
                    if (pos != (inst.param.subject_begin ? config.subject_begin : config.subject_end))
                        return failed();
                    break;
                case CompiledRegex::WordBoundary:
                    if (is_word_boundary(pos, config) != inst.param.word_boundary_positive)
                        return failed();
                    break;
                case CompiledRegex::LookAround:
                    if (lookaround(inst.param.lookaround, pos, config) != inst.param.lookaround.positive)
                        return failed();
                    break;
            }
        }
        return failed();
    }

    bool exec_program(Iterator pos, const ExecConfig& config)
    {
        kak_assert(m_threads.current_is_empty() and m_threads.next_is_empty());
        release_saves(m_captures);
        m_captures = -1;
        m_threads.ensure_initial_capacity();

        const int16_t first_inst = forward ? 0 : m_program.first_backward_inst;
        m_threads.push_current({first_inst, -1});

        const auto& start_desc = forward ? m_program.forward_start_desc : m_program.backward_start_desc;

        constexpr bool search = mode & RegexMode::Search;
        constexpr bool any_match = mode & RegexMode::AnyMatch;
        uint16_t current_step = -1;
        m_found_match = false;
        while (true) // Iterate on all codepoints and once at the end
        {
            if (++current_step == 0)
            {
                // We wrapped, avoid potential collision on inst.last_step by resetting them
                ConstArrayView<CompiledRegex::Instruction> instructions{m_program.instructions};
                instructions = forward ? instructions.subrange(0, m_program.first_backward_inst)
                                       : instructions.subrange(m_program.first_backward_inst);

                for (auto& inst : instructions)
                    inst.last_step = 0;
                current_step = 1; // step 0 is never valid
            }

            auto next = pos;
            Codepoint cp = pos != config.end ? codepoint(next, config) : -1;

            while (not m_threads.current_is_empty())
                step_thread(pos, cp, current_step, m_threads.pop_current(), config);

            if (pos == config.end or
                (m_threads.next_is_empty() and (not search or m_found_match)) or
                (m_found_match and any_match))
            {
                while (not m_threads.next_is_empty())
                    release_saves(m_threads.pop_next().saves);
                return m_found_match;
            }

            pos = next;
            if (search and not m_found_match)
            {
                if (start_desc and m_threads.next_is_empty())
                    to_next_start(pos, config, *start_desc);
                m_threads.push_next({first_inst, -1});
            }
            m_threads.swap_next();
        }
    }

    static void to_next_start(Iterator& start, const ExecConfig& config, const StartDesc& start_desc)
    {
        while (start != config.end)
        {
            static_assert(StartDesc::count <= 128, "start desc should be ascii only");
            if constexpr (forward)
            {
                const unsigned char c = *start;
                if (start_desc.map[(c < StartDesc::count) ? c : StartDesc::other])
                    return;
                utf8::to_next(start, config.end);
            }
            else
            {
                auto prev = utf8::previous(start, config.end);
                const unsigned char c = *prev;
                if (start_desc.map[(c < StartDesc::count) ? c : StartDesc::other])
                    return;
                start = prev;
            }
        }
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

    const CompiledRegex& m_program;

    struct DualThreadStack
    {
        bool current_is_empty() const { return m_current == m_next_begin; }
        bool next_is_empty() const { return m_next_end == m_next_begin; }

        void push_current(Thread thread) { m_data[decrement(m_current)] = thread; grow_ifn(true); }
        Thread pop_current() { return m_data[post_increment(m_current)]; }

        void push_next(Thread thread) { m_data[post_increment(m_next_end)] = thread; grow_ifn(false); }
        Thread pop_next() { return m_data[decrement(m_next_end)]; }

        void swap_next()
        {
            m_current = m_next_begin;
            m_next_begin = m_next_end;
        }

        void ensure_initial_capacity()
        {
            if (m_capacity != 0)
                return;

            constexpr int32_t initial_capacity = 64 / sizeof(Thread);
            static_assert(initial_capacity >= 4);
            m_data.reset(new Thread[initial_capacity]);
            m_capacity = initial_capacity;

        }

        void grow_ifn(bool pushed_current)
        {
            if (m_current != m_next_end)
                return;

            const auto new_capacity = m_capacity * 2;
            Thread* new_data = new Thread[new_capacity];
            Thread* old_data = m_data.get();
            std::rotate_copy(old_data, old_data + m_current, old_data + m_capacity, new_data);
            m_next_begin -= m_current;
            if ((pushed_current and m_next_begin == 0) or m_next_begin < 0)
                m_next_begin += m_capacity;
            m_next_end = m_capacity;
            m_current = 0;

            m_data.reset(new_data);
            m_capacity = new_capacity;
        }

    private:
        int32_t decrement(int32_t& index)
        {
            if (index == 0)
                index = m_capacity;
            return --index;
        }

        int32_t post_increment(int32_t& index)
        {
            auto res = index;
            if (++index == m_capacity)
                index = 0;
            return res;
        }

        std::unique_ptr<Thread[]> m_data;
        int32_t m_capacity = 0; // Maximum capacity should be 2*instruction count, so 65536
        int32_t m_current = 0;
        int32_t m_next_begin = 0;
        int32_t m_next_end = 0;
    };

    static constexpr bool forward = mode & RegexMode::Forward;

    DualThreadStack m_threads;
    Vector<Saves*, MemoryDomain::Regex> m_saves;
    int16_t m_first_free = -1;
    int16_t m_captures = -1;
    bool m_found_match = false;
};

}

#endif // regex_impl_hh_INCLUDED
