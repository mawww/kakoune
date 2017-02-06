hook global BufCreate .*\.(z|ba|c|k)?sh(rc|_profile)? %{
    set buffer filetype sh
}

add-highlighter -group / regions -default code -match-capture sh \
    double_string  %{(?<!\\)(?:\\\\)*\K"} %{(?<!\\)(?:\\\\)*"} '' \
    single_string %{(?<!\\)(?:\\\\)*\K'} %{'} '' \
    comment '(?<!\$)#' '$' '' \
    heredoc '<<-?(\w+)' '^\t*(\w+)$' ''

add-highlighter -group /sh/double_string fill string
add-highlighter -group /sh/single_string fill string
add-highlighter -group /sh/comment fill comment
add-highlighter -group /sh/heredoc fill string

%sh{
    # Grammar
    keywords="alias|bind|builtin|caller|case|cd|command|coproc|declare|do|done"
    keywords="${keywords}|echo|elif|else|enable|esac|exit|fi|for|function|help"
    keywords="${keywords}|if|in|let|local|logout|mapfile|printf|read|readarray"
    keywords="${keywords}|readonly|return|select|set|shift|source|test|then"
    keywords="${keywords}|time|type|typeset|ulimit|unalias|until|while"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=sh %{
        set window static_words '${keywords}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "add-highlighter -group /sh/code regex \b(${keywords})\b 0:keyword"
}

add-highlighter -group /sh/code regex [\[\]\(\)&|]{1,2} 0:operator
add-highlighter -group /sh/code regex (\w+)= 1:identifier
add-highlighter -group /sh/code regex ^\h*(\w+)\h*\(\) 1:identifier

add-highlighter -group /sh/code regex \$(\w+|\{.+?\}|#|@|\?|\$|!|-|\*) 0:value
add-highlighter -group /sh/double_string regex \$(\w+|\{.+?\}) 0:identifier

hook -group sh-highlight global WinSetOption filetype=sh %{ add-highlighter ref sh }
hook -group sh-highlight global WinSetOption filetype=(?!sh).* %{ remove-highlighter sh }
