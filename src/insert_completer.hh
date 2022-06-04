#ifndef insert_completer_hh_INCLUDED
#define insert_completer_hh_INCLUDED

#include "option_manager.hh"
#include "option.hh"
#include "display_buffer.hh"
#include "vector.hh"

#include "optional.hh"

namespace Kakoune
{

struct SelectionList;
struct Key;
class FaceRegistry;

struct InsertCompleterDesc
{
    enum Mode
    {
        Word,
        Option,
        Filename,
        Line
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
InsertCompleterDesc option_from_string(Meta::Type<InsertCompleterDesc>, StringView str);

inline StringView option_type_name(Meta::Type<InsertCompleterDesc>)
{
    return "completer";
}

using CompletionCandidate = std::tuple<String, String, String>;
using CompletionList = PrefixedList<String, CompletionCandidate>;

inline StringView option_type_name(Meta::Type<CompletionList>)
{
    return "completions";
}

struct InsertCompletion
{
    struct Candidate
    {
        String completion;
        String on_select;
        DisplayLine menu_entry;

        bool operator==(const Candidate& other) const { return completion == other.completion; }
        bool operator<(const Candidate& other) const { return completion < other.completion; }
    };
    using CandidateList = Vector<Candidate, MemoryDomain::Completion>;

    CandidateList candidates;
    BufferCoord begin;
    BufferCoord end;
    size_t timestamp = 0;

    bool is_valid() const { return not candidates.empty(); }
};

class InsertCompleter : public OptionManagerWatcher
{
public:
    InsertCompleter(Context& context);
    InsertCompleter(const InsertCompleter&) = delete;
    InsertCompleter& operator=(const InsertCompleter&) = delete;
    ~InsertCompleter();

    void select(int index, bool relative, Vector<Key>& keystrokes);
    void update(bool allow_implicit);
    void try_accept();
    void reset();

    void explicit_file_complete();
    void explicit_word_buffer_complete();
    void explicit_word_all_complete();
    void explicit_line_buffer_complete();
    void explicit_line_all_complete();

private:
    bool setup_ifn();

    template<typename Func>
    bool try_complete(Func complete_func);
    void on_option_changed(const Option& opt) override;

    void menu_show();
    bool has_candidate_selected() const;

    Context&            m_context;
    OptionManager&      m_options;
    const FaceRegistry& m_faces;
    InsertCompletion    m_completions;
    Vector<BufferRange> m_inserted_ranges;
    int                 m_current_candidate = -1;
    bool                m_enabled = true;

    using CompleteFunc = InsertCompletion (const SelectionList& sels,
                                           const OptionManager& options,
                                           const FaceRegistry& faces);
    CompleteFunc* m_explicit_completer = nullptr;
};

}

#endif // insert_completer_hh_INCLUDED
