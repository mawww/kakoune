hook global BufCreate .*\.(z|ba|c|k|mk)?sh(rc|_profile)? %{
    set-option buffer filetype sh
}

add-highlighter shared/ regions -default code -match-capture sh \
    double_string  %{(?<!\\)(?:\\\\)*\K"} %{(?<!\\)(?:\\\\)*"} '' \
    single_string %{(?<!\\)(?:\\\\)*\K'} %{'} '' \
    comment '(?<!\$)#' '$' '' \
    heredoc '<<-?(\w+)' '^\t*(\w+)$' ''

add-highlighter shared/sh/double_string fill string
add-highlighter shared/sh/single_string fill string
add-highlighter shared/sh/comment fill comment
add-highlighter shared/sh/heredoc fill string

evaluate-commands %sh{
    # Grammar
    keywords="alias|bind|builtin|caller|case|cd|command|coproc|declare|do|done"
    keywords="${keywords}|echo|elif|else|enable|esac|exit|fi|for|function|help"
    keywords="${keywords}|if|in|let|local|logout|mapfile|printf|read|readarray"
    keywords="${keywords}|readonly|return|select|set|shift|source|test|then"
    keywords="${keywords}|time|type|typeset|ulimit|unalias|until|while|break|continue"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=sh %{
        set-option window static_words '${keywords}'
    }" | tr '|' ':'

    # Highlight keywords
    printf %s "add-highlighter shared/sh/code regex \b(${keywords})\b 0:keyword"
}

add-highlighter shared/sh/code regex [\[\]\(\)&|]{1,2} 0:operator
add-highlighter shared/sh/code regex (\w+)= 1:variable
add-highlighter shared/sh/code regex ^\h*(\w+)\h*\(\) 1:variable

add-highlighter shared/sh/code regex \$(\w+|\{.+?\}|#|@|\?|\$|!|-|\*) 0:value
add-highlighter shared/sh/double_string regex \$(\w+|\{.+?\}) 0:value

hook -group sh-highlight global WinSetOption filetype=sh %{ add-highlighter window ref sh }
hook -group sh-highlight global WinSetOption filetype=(?!sh).* %{ remove-highlighter window/sh }
