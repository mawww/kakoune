#ifndef insert_completer_hh_INCLUDED
#define insert_completer_hh_INCLUDED

#include "buffer.hh"
#include "option_manager.hh"

namespace Kakoune
{

struct InsertCompletion
{
    BufferCoord begin;
    BufferCoord end;
    CandidateList  candidates;
    size_t         timestamp;

    bool is_valid() const { return not candidates.empty(); }
};

class InsertCompleter : public OptionManagerWatcher_AutoRegister
{
public:
    InsertCompleter(const Context& context);
    InsertCompleter(const InsertCompleter&) = delete;
    InsertCompleter& operator=(const InsertCompleter&) = delete;

    void select(int offset);
    void update();
    void reset();

    void explicit_file_complete();
    void explicit_word_complete();
    void explicit_line_complete();

private:
    bool setup_ifn();

    template<typename CompleteFunc>
    bool try_complete(CompleteFunc complete_func);
    void on_option_changed(const Option& opt) override;

    void menu_show();

    const Context&   m_context;
    InsertCompletion m_completions;
    CandidateList    m_matching_candidates;
    int              m_current_candidate = -1;
};

}

#endif // insert_completer_hh_INCLUDED

