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
    string   '"'      (?<!\\)(\\\\)*"      ''  \
    comment  (--[^>]) $                    ''  \
    comment \{-       -\}                  \{- \
    macro   ^\h*?\K#  (?<!\\)\n            ''

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

def -hidden haskell-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden haskell-indent-on-new-line %{
    eval -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K--\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # align to first clause
        try %{ exec -draft \; k x X s ^\h*(if|then|else)?\h*(([\w']+\h+)+=)?\h*(case\h+[\w']+\h+of|do|let|where)\h+\K.* <ret> s \`|.\' <ret> & }
        # filter previous line
        try %{ exec -draft k : haskell-filter-around-selections <ret> }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ exec -draft \; k x <a-k> ^\h*(if)|(case\h+[\w']+\h+of|do|let|where|[=(])$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group haskell-highlight global WinSetOption filetype=haskell %{ add-highlighter ref haskell }

hook global WinSetOption filetype=haskell %{
    set buffer completion_extra_word_char "'"
    hook window InsertEnd  .* -group haskell-hooks  haskell-filter-around-selections
    hook window InsertChar \n -group haskell-indent haskell-indent-on-new-line
}

hook -group haskell-highlight global WinSetOption filetype=(?!haskell).* %{ remove-highlighter haskell }

hook global WinSetOption filetype=(?!haskell).* %{
    remove-hooks window haskell-indent
    remove-hooks window haskell-hooks
}
