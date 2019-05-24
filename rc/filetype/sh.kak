hook global BufCreate .*\.(z|ba|c|k|mk)?sh(rc|_profile)? %{
    set-option buffer filetype sh
}

hook global WinSetOption filetype=sh %{
    require-module sh
    set-option window static_words %opt{sh_static_words}

    hook window ModeChange insert:.* -group sh-trim-indent sh-trim-indent
    hook window InsertChar \n -group sh-indent sh-indent-on-new-line
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window sh-.+ }
}

hook -group sh-highlight global WinSetOption filetype=sh %{
    add-highlighter window/sh ref sh
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/sh }
}

provide-module sh %[

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

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden sh-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden sh-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : sh-trim-indent <ret> }

        # indent after do
        try %{ execute-keys -draft <space> k x <a-k> do$ <ret> j <a-gt> }
        # deindent after done
        try %{ execute-keys -draft <space> k x <a-k> done$ <ret> <a-lt> j K <a-&> }

        # indent after then
        try %{ execute-keys -draft <space> k x <a-k> then$ <ret> j <a-gt> }
        # deindent after fi
        try %{ execute-keys -draft <space> k x <a-k> fi$ <ret> <a-lt> j K <a-&> }

        # indent after in
        try %{ execute-keys -draft <space> k x <a-k> in$ <ret> j <a-gt> }
        # deindent after esac
        try %{ execute-keys -draft <space> k x <a-k> esac$ <ret> <a-lt> j K <a-&> }

        # indent after )
        try %{ execute-keys -draft <space> k x <a-k> \)$ <ret> j <a-gt> }
        # deindent after ;;
        try %{ execute-keys -draft <space> k x <a-k> \;\;$ <ret> j <a-lt> }

        # function indent
        try %= execute-keys -draft <space> k x <a-k> \{$ <ret> j <a-gt> =
        # deindent at end of function
        try %= execute-keys -draft <space> k x <a-k> \}$ <ret> <a-lt> j K <a-&> =

    }
}

]
