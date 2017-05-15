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
    string   '(?<!\'\\)(?<!\')"'         (?<!\\)(\\\\)*" ''   \
    macro   ^\h*?\K#                     (?<!\\)\n       ''   \
    pragma  \{-#                         \#-\}           \{-  \
    comment \{-                            -\}           \{-  \
    comment --(?![!#$%&*+./<>?@\\\^|~=]) $               ''

add-highlighter -group /haskell/string  fill string
add-highlighter -group /haskell/comment fill comment
add-highlighter -group /haskell/pragma  fill meta
add-highlighter -group /haskell/macro   fill meta

add-highlighter -group /haskell/code regex (?<=\W)0x+[A-Fa-f0-9]+ 0:value
add-highlighter -group /haskell/code regex (?<=\W)\d+([.]\d+)? 0:value
add-highlighter -group /haskell/code regex \b(import|hiding|qualified)\b 0:keyword
add-highlighter -group /haskell/code regex \b(import|module)\b[^\n]+\b(as)\b 1:keyword 2:keyword
add-highlighter -group /haskell/code regex \b(class|data|default|deriving|infix|infixl|infixr|instance|module|newtype|pattern|type|where)\b 0:keyword
add-highlighter -group /haskell/code regex \b(case|do|else|if|in|let|mdo|of|proc|rec|then)\b 0:attribute

                                 # matches uppercase identifiers:  Monad Control.Monad
                                 # not non-space separated dot:    Just.const
add-highlighter -group /haskell/code regex \b([[:upper:]]['\w_]*\.)*[[:upper:]]['\w_]*(?!['\w_])(?![.[[:lower:]]) 0:variable

                                 # matches infix identifier: `mod` `Apa._T'M`
add-highlighter -group /haskell/code regex `\b([[:upper:]]['\w_]+\.)*[\w_]['\w_]*` 0:operator
                                 # matches imported operators: M.! M.. Control.Monad.>>
                                 # not operator keywords:      M... M.->
add-highlighter -group /haskell/code regex \b[[:upper:]]['\w_]*\.(?!([~=|:@\\]|<-|->|=>|\.\.|::)[^~<=>|:!?/.@$*&#%+\^\-\\])[~<=>|:!?/.@$*&#%+\^\-\\]+ 0:operator
                                 # matches dot: .
                                 # not possibly incomplete import:  a.
                                 # not other operators:             !. .!
add-highlighter -group /haskell/code regex (?<![\w~<=>|:!?/.@$*&#%+\^\-\\])\.(?![~<=>|:!?/.@$*&#%+\^\-\\]) 0:operator
                                 # matches other operators: ... > < <= ^ <*> <$> etc
                                 # not dot: .
                                 # not operator keywords:  @ .. -> :: ~
add-highlighter -group /haskell/code regex (?<![~<=>|:!?/.@$*&#%+\^\-\\])(?!([~=|:.@\\]|<-|->|=>|\.\.|::)[^~<=>|:!?/.@$*&#%+\^\-\\])[~<=>|:!?/.@$*&#%+\^\-\\]+ 0:operator

                                 # matches operator keywords: @ ->
add-highlighter -group /haskell/code regex (?<![~<=>|:!?/.@$*&#%+\^\-\\])(@|~|<-|->|=>|::|=|:|[|])(?![~<=>|:!?/.@$*&#%+\^\-\\]) 1:keyword
                                 # matches: forall [..prenex..] .
                                 # not the prenex
add-highlighter -group /haskell/code regex \b(forall)\b[^.\n]*?(\.) 1:keyword 2:keyword

                                 # matches 'x' '\\' '\'' '\n' '\0'
                                 # not incomplete literals: '\'
                                 # not valid identifiers:   w' _'
add-highlighter -group /haskell/code regex (?<!\w)'([^\\]|[\\]['"\w\d\\])' 0:string
    # this has to come after operators so '-' etc is correct

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
