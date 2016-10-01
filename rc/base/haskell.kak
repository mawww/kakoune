# http://haskell.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-haskell %{
    set buffer filetype haskell
}

hook global BufCreate .*[.](hs) %{
    set buffer filetype haskell
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code haskell \
    string   '"'     (?<!\\)(\\\\)*"      '' \
    comment  (--) $                       '' \
    comment \{-   -\}                    \{- \
    macro   ^\h*?\K# (?<!\\)\n            ''

addhl -group /haskell/string  fill string
addhl -group /haskell/comment fill comment
addhl -group /haskell/macro   fill meta

addhl -group /haskell/code regex \b(import)\b 0:meta
addhl -group /haskell/code regex \b(True|False)\b 0:value
addhl -group /haskell/code regex \b(as|case|class|data|default|deriving|do|else|hiding|if|in|infix|infixl|infixr|instance|let|module|newtype|of|qualified|then|type|where)\b 0:keyword
addhl -group /haskell/code regex \b(Int|Integer|Char|Bool|Float|Double|IO|Void|Addr|Array|String)\b 0:type

# Commands
# ‾‾‾‾‾‾‾‾

# http://en.wikibooks.org/wiki/Haskell/Indentation

def -hidden _haskell_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _haskell_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # align to first clause
        try %{ exec -draft <space> k x X s ^\h*(if|then|else)?\h*(([\w']+\h+)+=)?\h*(case\h+[\w']+\h+of|do|let|where)\h+\K.* <ret> s \`|.\' <ret> & }
        # filter previous line
        try %{ exec -draft k : _haskell_filter_around_selections <ret> }
        # copy -- comments prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K--\h* <ret> y j p }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ exec -draft <space> k x <a-k> ^\h*(if)|(case\h+[\w']+\h+of|do|let|where|[=(])$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group haskell-highlight global WinSetOption filetype=haskell %{ addhl ref haskell }

hook global WinSetOption filetype=haskell %{
    hook window InsertEnd  .* -group haskell-hooks  _haskell_filter_around_selections
    hook window InsertChar \n -group haskell-indent _haskell_indent_on_new_line
}

hook -group haskell-highlight global WinSetOption filetype=(?!haskell).* %{ rmhl haskell }

hook global WinSetOption filetype=(?!haskell).* %{
    rmhooks window haskell-indent
    rmhooks window haskell-hooks
}
