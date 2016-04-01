#ifndef insert_completer_hh_INCLUDED
#define insert_completer_hh_INCLUDED

#include "option_manager.hh"
#include "display_buffer.hh"
#include "vector.hh"

#include "optional.hh"

namespace Kakoune
{

class Buffer;
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

using CompletionCandidate = std::tuple<String, String, String>;
using CompletionList = PrefixedList<String, CompletionCandidate>;

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

    using CandidateList = Vector<Candidate>;

    ByteCoord begin;
    ByteCoord end;
    CandidateList candidates;
    size_t timestamp;

    bool is_valid() const { return not candidates.empty(); }
};

class InsertCompleter : public OptionManagerWatcher
{
public:
    InsertCompleter(const Context& context);
    InsertCompleter(const InsertCompleter&) = delete;
    InsertCompleter& operator=(const InsertCompleter&) = delete;
    ~InsertCompleter();

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

    const Context&   m_context;
    OptionManager&   m_options;
    InsertCompletion m_completions;
    int              m_current_candidate = -1;

    using CompleteFunc = InsertCompletion (const Buffer&, ByteCoord, const OptionManager& options);
    CompleteFunc* m_explicit_completer = nullptr;
};

}

#endif // insert_completer_hh_INCLUDED
