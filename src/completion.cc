#include "completion.hh"
#include "file.hh"
#include "context.hh"
#include "option_types.hh"
#include "regex.hh"

namespace Kakoune
{

Completions shell_complete(const Context& context, CompletionFlags flags,
                           StringView prefix, ByteCount cursor_pos)
{
    ByteCount word_start = 0;
    ByteCount word_end = 0;

    bool command = true;
    const ByteCount len = prefix.length();
    for (ByteCount pos = 0; pos < cursor_pos and pos < len;)
    {
        command = (pos == 0 or prefix[pos-1] == ';' or prefix[pos-1] == '|' or
                   (pos > 1 and prefix[pos-1] == '&' and prefix[pos-2] == '&'));
        while (pos != len and is_horizontal_blank(prefix[pos]))
            ++pos;
        word_start = pos;
        while (pos != len and not is_horizontal_blank(prefix[pos]))
            ++pos;
        word_end = pos;
    }
    Completions completions{word_start, word_end};
    if (command)
        completions.candidates = complete_command(prefix.substr(word_start, word_end),
                                                  cursor_pos - word_start);
    else
        completions.candidates = complete_filename(prefix.substr(word_start, word_end),
                                                   context.options()["ignored_files"].get<Regex>(),
                                                   cursor_pos - word_start);
    return completions;
}

}
