hook global BufCreate .*\.(z|ba|c|k|mk)?sh(rc|_profile)? %{
    set-option buffer filetype sh
}

add-highlighter shared/sh regions
add-highlighter shared/sh/code default-region group
add-highlighter shared/sh/double_string region  %{(?<!\\)(?:\\\\)*\K"} %{(?<!\\)(?:\\\\)*"} group
add-highlighter shared/sh/single_string region %{(?<!\\)(?:\\\\)*\K'} %{'} fill string
add-highlighter shared/sh/comment region '(?<!\$)#' '$' fill comment
add-highlighter shared/sh/heredoc region -match-capture '<<-?(\w+)' '^\t*(\w+)$' fill string

add-highlighter shared/sh/double_string/fill fill string

evaluate-commands %sh{
    # Grammar
    keywords="alias bind builtin caller case cd command coproc declare do done
              echo elif else enable esac exit fi for function help
              if in let local logout mapfile printf read readarray
              readonly return select set shift source test then
              time type typeset ulimit unalias until while break continue"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=sh %{
        set-option window static_words $(join "${keywords}" ' ')
    }"

    # Highlight keywords
    printf %s "add-highlighter shared/sh/code/ regex \b($(join "${keywords}" '|'))\b 0:keyword"
}

add-highlighter shared/sh/code/operators regex [\[\]\(\)&|]{1,2} 0:operator
add-highlighter shared/sh/code/variable regex (\w+)= 1:variable
add-highlighter shared/sh/code/function regex ^\h*(\w+)\h*\(\) 1:function

add-highlighter shared/sh/code/expansion regex \$(\w+|\{.+?\}|#|@|\?|\$|!|-|\*) 0:value
add-highlighter shared/sh/double_string/expansion regex \$(\w+|\{.+?\}) 0:value

hook -group sh-highlight global WinSetOption filetype=sh %{ add-highlighter window/sh ref sh }
hook -group sh-highlight global WinSetOption filetype=(?!sh).* %{ remove-highlighter window/sh }
