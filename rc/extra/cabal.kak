# http://haskell.org/cabal
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](cabal) %{
    set buffer filetype cabal
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code cabal \
    comment  (--) $                     '' \
    comment \{-   -\}                  \{-

add-highlighter -group /cabal/comment fill comment

add-highlighter -group /cabal/code regex \b(true|false)\b|(([<>]?=?)?\d+(\.\d+)+) 0:value
add-highlighter -group /cabal/code regex \b(if|else)\b 0:keyword
add-highlighter -group /cabal/code regex ^\h*([A-Za-z][A-Za-z0-9_-]*)\h*: 1:variable

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden cabal-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden cabal-indent-on-new-line %[
    eval -draft -itersel %[
        # copy '#' comment prefix and following white spaces
        try %[ exec -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P ]
        # preserve previous line indent
        try %[ exec -draft \; K <a-&> ]
        # filter previous line
        try %[ exec -draft k : cabal-filter-around-selections <ret> ]
        # indent after lines ending with { or :
        try %[ exec -draft <space> k <a-x> <a-k> [:{]$ <ret> j <a-gt> ]
    ]
]

def -hidden cabal-indent-on-opening-curly-brace %[
    eval -draft -itersel %[
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ exec -draft h <a-F> ) M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
    ]
]

def -hidden cabal-indent-on-closing-curly-brace %[
    eval -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ exec -draft <a-h> <a-k> ^\h+\}$ <ret> h m s \`|.\'<ret> 1<a-&> ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group cabal-highlight global WinSetOption filetype=cabal %{ add-highlighter ref cabal }

hook global WinSetOption filetype=cabal %[
    hook window InsertEnd  .* -group cabal-hooks  cabal-filter-around-selections
    hook window InsertChar \n -group cabal-indent cabal-indent-on-new-line
    hook window InsertChar \{ -group cabal-indent cabal-indent-on-opening-curly-brace
    hook window InsertChar \} -group cabal-indent cabal-indent-on-closing-curly-brace
]

hook -group cabal-highlight global WinSetOption filetype=(?!cabal).* %{ remove-highlighter cabal }

hook global WinSetOption filetype=(?!cabal).* %{
    remove-hooks window cabal-indent
    remove-hooks window cabal-hooks
}
