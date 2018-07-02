# http://scala-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](scala) %{
    set-option buffer filetype scala
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/scala regions
add-highlighter shared/scala/code default-region group
add-highlighter shared/scala/string       region '"' (?<!\\)(\\\\)*"   fill string
add-highlighter shared/scala/literal      region `    `                fill variable
add-highlighter shared/scala/comment      region -recurse /[*] /[*] [*]/  fill comment
add-highlighter shared/scala/line_comment region //   $                fill comment

# Keywords are collected at
# http://tutorialspoint.com/scala/scala_basic_syntax.htm

add-highlighter shared/scala/code/ regex (?:\b|\W)(@\w+|import|package)\b 0:meta
add-highlighter shared/scala/code/ regex \b(true|false|null)\b 0:value
add-highlighter shared/scala/code/ regex \b(?:class|extends|with)\s+(\w+) 0:type
add-highlighter shared/scala/code/ regex \b([A-Z]\w*)\b 0:type
add-highlighter shared/scala/code/ regex (?:def|var|val)\s+(\w+) 0:variable
add-highlighter shared/scala/code/ regex \b(become|case|catch|class|def|do|else|extends|final|finally|for|forSome|goto|if|initialize|macro|match|new|object|onTransition|return|startWith|stay|this|super|throw|trait|try|unbecome|using|val|var|when|while|with|yield)\b 0:keyword
add-highlighter shared/scala/code/ regex \b(abstract|final|implicit|implicitly|lazy|override|private|protected|require|sealed)\b 0:attribute
add-highlighter shared/scala/code/ regex (\[|\]|=>|<:|:>|=:=|::|&&|\|\|) 0:operator

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

hook -group scala-highlight global WinSetOption filetype=scala %{ add-highlighter window/scala ref scala }

hook global WinSetOption filetype=scala %[
    hook window ModeChange insert:.* -group scala-hooks  scala-filter-around-selections
    hook window InsertChar \n -group scala-indent scala-indent-on-new-line
    hook window InsertChar \} -group scala-indent scala-indent-on-closing-curly-brace
]

hook -group scala-highlight global WinSetOption filetype=(?!scala).* %{ remove-highlighter window/scala }

hook global WinSetOption filetype=(?!scala).* %{
    remove-hooks window scala-indent
    remove-hooks window scala-hooks
}
