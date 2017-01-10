# http://haskell.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](hs) %{
    set buffer filetype haskell
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code haskell \
    string   '"'     (?<!\\)(\\\\)*"      '' \
    comment  (--) $                       '' \
    comment \{-   -\}                    \{- \
    macro   ^\h*?\K# (?<!\\)\n            ''

add-highlighter -group /haskell/string  fill string
add-highlighter -group /haskell/comment fill comment
add-highlighter -group /haskell/macro   fill meta

add-highlighter -group /haskell/code regex \b(import)\b 0:meta
add-highlighter -group /haskell/code regex \b(True|False)\b 0:value
add-highlighter -group /haskell/code regex \b(as|case|class|data|default|deriving|do|else|hiding|if|in|infix|infixl|infixr|instance|let|module|newtype|of|qualified|then|type|where)\b 0:keyword
add-highlighter -group /haskell/code regex \b(Int|Integer|Char|Bool|Float|Double|IO|Void|Addr|Array|String)\b 0:type

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
        try %{ exec -draft k x s ^\h*\K--\h* <ret> y gh j P }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ exec -draft <space> k x <a-k> ^\h*(if)|(case\h+[\w']+\h+of|do|let|where|[=(])$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group haskell-highlight global WinSetOption filetype=haskell %{ add-highlighter ref haskell }

hook global WinSetOption filetype=haskell %{
    hook window InsertEnd  .* -group haskell-hooks  _haskell_filter_around_selections
    hook window InsertChar \n -group haskell-indent _haskell_indent_on_new_line
}

hook -group haskell-highlight global WinSetOption filetype=(?!haskell).* %{ remove-highlighter haskell }

hook global WinSetOption filetype=(?!haskell).* %{
    remove-hooks window haskell-indent
    remove-hooks window haskell-hooks
}
