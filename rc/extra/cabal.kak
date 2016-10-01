# http://haskell.org/cabal
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-cabal %{
    set buffer filetype cabal
}

hook global BufCreate .*[.](cabal) %{
    set buffer filetype cabal
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code cabal \
    comment  (--) $                     '' \
    comment \{-   -\}                  \{-

addhl -group /cabal/comment fill comment

addhl -group /cabal/code regex \b(true|false)\b|(([<>]?=?)?\d+(\.\d+)+) 0:value
addhl -group /cabal/code regex \b(if|else)\b 0:keyword
addhl -group /cabal/code regex ^\h*([A-Za-z][A-Za-z0-9_-]*)\h*: 1:identifier

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _cabal_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _cabal_indent_on_new_line %[
    eval -draft -itersel %[
        # preserve previous line indent
        try %[ exec -draft <space> K <a-&> ]
        # filter previous line
        try %[ exec -draft k : _cabal_filter_around_selections <ret> ]
        # copy '#' comment prefix and following white spaces
        try %[ exec -draft k x s ^\h*\K#\h* <ret> y j p ]
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

hook -group cabal-highlight global WinSetOption filetype=cabal %{ addhl ref cabal }

hook global WinSetOption filetype=cabal %[
    hook window InsertEnd  .* -group cabal-hooks  _cabal_filter_around_selections
    hook window InsertChar \n -group cabal-indent _cabal_indent_on_new_line
    hook window InsertChar \{ -group cabal-indent _cabal_indent_on_opening_curly_brace
    hook window InsertChar \} -group cabal-indent _cabal_indent_on_closing_curly_brace
]

hook -group cabal-highlight global WinSetOption filetype=(?!cabal).* %{ rmhl cabal }

hook global WinSetOption filetype=(?!cabal).* %{
    rmhooks window cabal-indent
    rmhooks window cabal-hooks
}
