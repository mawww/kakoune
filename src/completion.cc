#include "completion.hh"
#include "file.hh"
#include "context.hh"

namespace Kakoune
{

Completions shell_complete(const Context& context, CompletionFlags flags,
                           const String& prefix, ByteCount cursor_pos)
{
    ByteCount word_start = 0;
    ByteCount word_end = 0;

    bool first = true;
    const ByteCount len = prefix.length();
    for (ByteCount pos = 0; pos < cursor_pos;)
    {
        if (pos != 0)
            first = false;
        while (pos != len and is_blank(prefix[pos]))
            ++pos;
        word_start = pos;
        while (pos != len and not is_blank(prefix[pos]))
            ++pos;
        word_end = pos;
    }
    Completions completions{word_start, word_end};
    if (first)
        completions.candidates = complete_command(prefix.substr(word_start, word_end),
                                                  cursor_pos - word_start);
    else
        completions.candidates = complete_filename(prefix.substr(word_start, word_end),
                                                   context.options()["ignored_files"].get<Regex>(), 
                                                   cursor_pos - word_start);
    return completions;
}

}
