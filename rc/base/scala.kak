# http://scala-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](scala) %{
    set-option buffer filetype scala
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code scala \
    string  '"' (?<!\\)(\\\\)*"         '' \
    literal  `    `                     '' \
    comment  //   $                     '' \
    comment /[*] [*]/                 /[*]

add-highlighter shared/scala/string  fill string
add-highlighter shared/scala/literal fill variable
add-highlighter shared/scala/comment fill comment

# Keywords are collected at
# http://tutorialspoint.com/scala/scala_basic_syntax.htm

add-highlighter shared/scala/code regex \b(import|package)\b 0:meta
add-highlighter shared/scala/code regex \b(this|true|false|null)\b 0:value
add-highlighter shared/scala/code regex \b(become|case|catch|class|def|do|else|extends|final|finally|for|forSome|goto|if|initialize|macro|match|new|object|onTransition|return|startWith|stay|throw|trait|try|unbecome|using|val|var|when|while|with|yield)\b 0:keyword
add-highlighter shared/scala/code regex \b(abstract|final|implicit|implicitly|lazy|override|private|protected|require|sealed|super)\b 0:attribute
add-highlighter shared/scala/code regex \b(⇒|=>|<:|:>|=:=|::|&&|\|\|)\b 0:operator
add-highlighter shared/scala/code regex "'[_A-Za-z0-9$]+" 0:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden scala-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden scala-indent-on-new-line %[
    evaluate-commands -draft -itersel %[
        # copy // comments prefix and following white spaces
        try %[ execute-keys -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P ]
        # preserve previous line indent
        try %[ execute-keys -draft \; K <a-&> ]
        # filter previous line
        try %[ execute-keys -draft k : scala-filter-around-selections <ret> ]
        # indent after lines ending with {
        try %[ execute-keys -draft k <a-x> <a-k> \{$ <ret> j <a-gt> ]
    ]
]

define-command -hidden scala-indent-on-closing-curly-brace %[
    evaluate-commands -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ execute-keys -draft <a-h> <a-k> ^\h+\}$ <ret> m s \A|.\z <ret> 1<a-&> ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group scala-highlight global WinSetOption filetype=scala %{ add-highlighter window ref scala }

hook global WinSetOption filetype=scala %[
    hook window InsertEnd  .* -group scala-hooks  scala-filter-around-selections
    hook window InsertChar \n -group scala-indent scala-indent-on-new-line
    hook window InsertChar \} -group scala-indent scala-indent-on-closing-curly-brace
]

hook -group scala-highlight global WinSetOption filetype=(?!scala).* %{ remove-highlighter window/scala }

hook global WinSetOption filetype=(?!scala).* %{
    remove-hooks window scala-indent
    remove-hooks window scala-hooks
}
