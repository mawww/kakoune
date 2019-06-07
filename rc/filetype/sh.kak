hook global BufCreate .*\.(z|ba|c|k|mk)?sh(rc|_profile)? %{
    set-option buffer filetype sh
}

hook global WinSetOption filetype=sh %{
    require-module sh
    set-option window static_words %opt{sh_static_words}
}

hook -group sh-highlight global WinSetOption filetype=sh %{
    add-highlighter window/sh ref sh
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/sh }
}

provide-module sh %[

declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient
declare-option -hidden int explain_shell_cols

add-highlighter shared/sh regions
add-highlighter shared/sh/code default-region group
add-highlighter shared/sh/double_string region  %{(?<!\\)(?:\\\\)*\K"} %{(?<!\\)(?:\\\\)*"} group
add-highlighter shared/sh/single_string region %{(?<!\\)(?:\\\\)*\K'} %{'} fill string
add-highlighter shared/sh/expansion region '\$\{' '\}|\n' fill value
add-highlighter shared/sh/comment region '(?<!\$)(?<!\$\{)#' '$' fill comment
add-highlighter shared/sh/heredoc region -match-capture '<<-?''?(\w+)''?' '^\t*(\w+)$' fill string

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
    printf %s\\n "declare-option str-list sh_static_words $(join "${keywords}" ' ')"

    # Highlight keywords
    printf %s "add-highlighter shared/sh/code/ regex \b($(join "${keywords}" '|'))\b 0:keyword"
}

add-highlighter shared/sh/code/operators regex [\[\]\(\)&|]{1,2} 0:operator
add-highlighter shared/sh/code/variable regex (\w+)= 1:variable
add-highlighter shared/sh/code/function regex ^\h*(\w+)\h*\(\) 1:function

add-highlighter shared/sh/code/unscoped_expansion regex \$(\w+|#|@|\?|\$|!|-|\*) 0:value
add-highlighter shared/sh/double_string/expansion regex \$(\w+|\{.+?\}) 0:value

define-command -docstring "Explain the selected shell command (online api)" explain-shell %{ try %{
    evaluate-commands -try-client %opt{docsclient} %{
        set-option global explain_shell_cols %val{window_width}
    }
    evaluate-commands %sh{
        sel=$(echo ${kak_selection} | sed 's/^[ \t\v\f]*//;s/[ \t\v\f]*$//')
        expl=$(mktemp "${TMPDIR:-/tmp}"/kak-explain-shell-XXXXXX)
        cols=$((${kak_opt_explain_shell_cols}-5))
        curl -Gs "https://www.mankier.com/api/explain/?cols=${cols}" \
            --data-urlencode "q=${sel}" -o ${expl}
        retval=$?
        if [ "${retval}" -eq 0 ]; then
              printf %s\\n "evaluate-commands -try-client '${kak_opt_docsclient}' %{
                  edit -scratch *explain-shell*
                  execute-keys 'gj|cat<space>${expl}<ret>gj'
                  nop %sh{rm -f ${expl}}
              }"
        else
          printf %s\\n "
              echo -markup %{{Error}explain-shell '${sel}' failed: see *debug* buffer for details}
              nop %sh{rm -f ${expl}}
          "
        fi
    }
}}

]
