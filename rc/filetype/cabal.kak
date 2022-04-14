# http://haskell.org/cabal
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](cabal) %{
    set-option buffer filetype cabal
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=cabal %[
    require-module cabal

    hook window ModeChange pop:insert:.* -group cabal-trim-indent cabal-trim-indent
    hook window InsertChar \n -group cabal-insert cabal-insert-on-new-line
    hook window InsertChar \n -group cabal-indent cabal-indent-on-new-line
    hook window InsertChar \{ -group cabal-indent cabal-indent-on-opening-curly-brace
    hook window InsertChar \} -group cabal-indent cabal-indent-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window cabal-.+ }
]

hook -group cabal-highlight global WinSetOption filetype=cabal %{
    add-highlighter window/cabal ref cabal
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/cabal }
}


provide-module cabal %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/cabal regions
add-highlighter shared/cabal/code default-region group
add-highlighter shared/cabal/line_comment region (--) $ fill comment
add-highlighter shared/cabal/comment region -recurse \{- \{- -\} fill comment

add-highlighter shared/cabal/code/ regex \b(true|false)\b|(([<>]?=?)?\d+(\.\d+)+) 0:value
add-highlighter shared/cabal/code/ regex \b(if|else)\b 0:keyword
add-highlighter shared/cabal/code/ regex ^\h*([A-Za-z][A-Za-z0-9_-]*)\h*: 1:variable

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden cabal-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden cabal-insert-on-new-line %[
    evaluate-commands -draft -itersel %[
        # copy '--' comment prefix and following white spaces
        try %[ execute-keys -draft k x s ^\h*\K--\h* <ret> y gh j P ]
    ]
]

define-command -hidden cabal-indent-on-new-line %[
    evaluate-commands -draft -itersel %[
        # preserve previous line indent
        try %[ execute-keys -draft <semicolon> K <a-&> ]
        # filter previous line
        try %[ execute-keys -draft k : cabal-trim-indent <ret> ]
        # indent after lines ending with { or :
        try %[ execute-keys -draft , k x <a-k> [:{]$ <ret> j <a-gt> ]
        # deindent closing brace when after cursor
        try %[ execute-keys -draft x <a-k> \h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> ]
    ]
]

define-command -hidden cabal-indent-on-opening-curly-brace %[
    evaluate-commands -draft -itersel %[
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ execute-keys -draft h <a-F> ) M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
    ]
]

define-command -hidden cabal-indent-on-closing-curly-brace %[
    evaluate-commands -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ execute-keys -draft <a-h> <a-k> ^\h+\}$ <ret> h m s \A|.\z<ret> 1<a-&> ]
    ]
]

]
