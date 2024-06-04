# Adapted from the file created by Daniel Lewan TeddyDD

hook global BufCreate "(.+\.(groovy|gvy|gy|gsh|gradle))|.+[Jj]enkinsfile.*" %{
    set-option buffer filetype groovy
}

hook global WinSetOption filetype=groovy %{
    require-module groovy

    set-option window static_words %opt{groovy_static_words}

    hook window ModeChange pop:insert:.* -group groovy-trim-indent %{ try %{ execute-keys -draft xs^\h+$<ret>d } }
    hook window InsertChar \n -group groovy-insert groovy-insert-on-new-line
    hook window InsertChar \n -group groovy-indent groovy-indent-on-new-line
    hook window InsertChar \{ -group groovy-indent groovy-indent-on-opening-curly-brace
    hook window InsertChar \} -group groovy-indent groovy-indent-on-closing-curly-brace
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window groovy-.+ }
}

hook -group groovy-highlight global WinSetOption filetype=groovy %{
    add-highlighter window/groovy ref groovy
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/groovy }
}

provide-module groovy %§

add-highlighter shared/groovy regions

add-highlighter shared/groovy/code default-region group
add-highlighter shared/groovy/triple_quote region '"{3}'   (?<!\\)(\\\\)*"{3}  fill string
add-highlighter shared/groovy/double_string region '"'   (?<!\\)(\\\\)*"  fill string
add-highlighter shared/groovy/single_string region "'"   (?<!\\)(\\\\)*'  fill string
add-highlighter shared/groovy/comment1 region '/\*[^*]?' '\*/' fill comment
add-highlighter shared/groovy/comment2 region '//[^/]?' $ fill comment
add-highlighter shared/groovy/shellbang region '#!.+' $ fill comment
add-highlighter shared/groovy/dollar_string region "\$/" "(?<!\$)/\$"   fill string
# add-highlighter shared/groovy/code/identifiers regex '\b[$_]?[a-zA-Z0-9_]+\b' 0:variable
add-highlighter shared/groovy/code/declaration regex "(?<typ>\w+)(?:\[.*?\])?\s+(\$?\w+)\s=" typ:type
add-highlighter shared/groovy/code/numbers regex '\b[-+]?0x[A-Fa-f0-9_]+[.A-Fa-f0-9_p]*[lLiDgGIF]?|\b[-+]?[\d]+b?[.p_\dEe]*[lLiDgGIF]?' 0:value
add-highlighter shared/groovy/slashy_string region "\b/\w" "(?<!\\)\w/\b"   fill string

evaluate-commands %sh{
  keywords="as|assert|break|case|catch|class|const|continue|def|default"
  keywords="${keywords}|do|else|enum|extends|finally|for|goto|if|implements|import|in"
  keywords="${keywords}|instanceof|interface|new|package|return|super|switch|this|throw"
  keywords="${keywords}|throws|trait|try|while"
  builtins="true|false|null"
  types="byte|char|short|int|long|BigInteger|float|double|BigDecimal|boolean"

  printf %s "
    add-highlighter shared/groovy/code/keyword regex \b(${keywords})\b 0:keyword
    add-highlighter shared/groovy/code/builtin regex \b(${builtins})\b 0:builtin
    add-highlighter shared/groovy/code/types   regex \b(${types})\b    0:type

    declare-option str-list groovy_static_words \"${keywords}|${types}|${builtins}\"
  "
}

define-command -hidden groovy-insert-on-new-line %[
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K/{2,}\h* <ret> y<c-o>P<esc> }
]

define-command -hidden groovy-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        # indent after lines ending with { or (
        try %[ execute-keys -draft kx <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ execute-keys -draft [( <a-k> \A\([^\n]+\n[^\n]*\n?\z <ret> s \A\(\h*.|.\z <ret> '<a-;>' & }
        # indent after a switch's case/default statements
        try %[ execute-keys -draft kx <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
        # indent after keywords
        try %[ execute-keys -draft <semicolon><a-F>)MB <a-k> \A(if|else|while|for|try|catch)\h*\(.*\)\h*\n\h*\n?\z <ret> s \A|.\z <ret> 1<a-&>1<a-space><a-gt> ]
        # deindent closing brace(s) when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
    =
~

define-command -hidden groovy-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden groovy-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]
§
