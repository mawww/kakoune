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
add-highlighter -group /cabal/code regex ^\h*([A-Za-z][A-Za-z0-9_-]*)\h*: 1:identifier

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _cabal_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _cabal_indent_on_new_line %[
    eval -draft -itersel %[
        # copy '#' comment prefix and following white spaces
        try %[ exec -draft k X<a-s><a-space> s ^\h*#\h* <ret> y gh j P ]
        # preserve previous line indent
        try %[ exec -draft <space> K <a-&> ]
        # filter previous line
        try %[ exec -draft k : _cabal_filter_around_selections <ret> ]
        # indent after lines ending with { or :
        try %[ exec -draft <space> k x <a-k> [:{]$ <ret> j <a-gt> ]
    ]
]

def -hidden _cabal_indent_on_opening_curly_brace %[
    eval -draft -itersel %[
        # align indent with opening paren when { is entered on a new line after the closing paren
        try %[ exec -draft h <a-F> ) M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
    ]
]

def -hidden _cabal_indent_on_closing_curly_brace %[
    eval -draft -itersel %[
        # align to opening curly brace when alone on a line
        try %[ exec -draft <a-h> <a-k> ^\h+\}$ <ret> h m s \`|.\'<ret> 1<a-&> ]
    ]
]

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group cabal-highlight global WinSetOption filetype=cabal %{ add-highlighter ref cabal }

hook global WinSetOption filetype=cabal %[
    hook window InsertEnd  .* -group cabal-hooks  _cabal_filter_around_selections
    hook window InsertChar \n -group cabal-indent _cabal_indent_on_new_line
    hook window InsertChar \{ -group cabal-indent _cabal_indent_on_opening_curly_brace
    hook window InsertChar \} -group cabal-indent _cabal_indent_on_closing_curly_brace
]

hook -group cabal-highlight global WinSetOption filetype=(?!cabal).* %{ remove-highlighter cabal }

hook global WinSetOption filetype=(?!cabal).* %{
    remove-hooks window cabal-indent
    remove-hooks window cabal-hooks
}
