#ifndef insert_completer_hh_INCLUDED
#define insert_completer_hh_INCLUDED

#include "option_manager.hh"
#include "display_buffer.hh"
#include "vector.hh"

#include "optional.hh"

namespace Kakoune
{

struct SelectionList;
struct Key;

struct InsertCompleterDesc
{
    enum Mode
    {
        Word,
        Option,
        Filename
    };

    bool operator==(const InsertCompleterDesc& other) const
    { return mode == other.mode and param == other.param; }

    bool operator!=(const InsertCompleterDesc& other) const
    { return not (*this == other); }

    Mode mode;
    Optional<String> param;
};

using InsertCompleterDescList = Vector<InsertCompleterDesc, MemoryDomain::Options>;

String option_to_string(const InsertCompleterDesc& opt);
void option_from_string(StringView str, InsertCompleterDesc& opt);

template<> struct option_type_name<InsertCompleterDesc>
{
    static constexpr StringView name() { return "completer"; }
};

using CompletionCandidate = std::tuple<String, String, String>;
using CompletionList = PrefixedList<String, CompletionCandidate>;

template<> struct option_type_name<CompletionList>
{
    static constexpr StringView name() { return "completions"; }
};

struct InsertCompletion
{
    struct Candidate
    {
        String completion;
        String docstring;
        DisplayLine menu_entry;

        bool operator==(const Candidate& other) const { return completion == other.completion; }
        bool operator<(const Candidate& other) const { return completion < other.completion; }
    };

    using CandidateList = Vector<Candidate, MemoryDomain::Completion>;

    CandidateList candidates;
    BufferCoord begin;
    BufferCoord end;
    size_t timestamp;

    InsertCompletion() : timestamp{0} {}

    InsertCompletion(CandidateList candidates,
                     BufferCoord begin, BufferCoord end,
                     size_t timestamp)
      : candidates{std::move(candidates)}, begin{begin}, end{end},
        timestamp{timestamp} {}

    bool is_valid() const { return not candidates.empty(); }
};

class InsertCompleter : public OptionManagerWatcher
{
public:
    InsertCompleter(Context& context);
    InsertCompleter(const InsertCompleter&) = delete;
    InsertCompleter& operator=(const InsertCompleter&) = delete;
    ~InsertCompleter() override;

    void select(int offset, Vector<Key>& keystrokes);
    void update();
    void reset();

    void explicit_file_complete();
    void explicit_word_complete();
    void explicit_line_complete();

private:
    bool setup_ifn();

    template<typename Func>
    bool try_complete(Func complete_func);
    void on_option_changed(const Option& opt) override;

    void menu_show();

    Context&         m_context;
    OptionManager&   m_options;
    InsertCompletion m_completions;
    int              m_current_candidate = -1;

    using CompleteFunc = InsertCompletion (const SelectionList& sels, const OptionManager& options);
    CompleteFunc* m_explicit_completer = nullptr;
};

}

#endif // insert_completer_hh_INCLUDED
