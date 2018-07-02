# http://haskell.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](hs) %{
    set-option buffer filetype haskell
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/haskell regions
add-highlighter shared/haskell/code default-region group
add-highlighter shared/haskell/string       region (?<!'\\)(?<!')"                 (?<!\\)(\\\\)*"  fill string
add-highlighter shared/haskell/macro        region ^\h*?\K#                        (?<!\\)\n        fill meta
add-highlighter shared/haskell/pragma       region -recurse \{- \{-#               '#-\}'           fill meta
add-highlighter shared/haskell/comment      region -recurse \{- \{-                  -\}            fill comment
add-highlighter shared/haskell/line_comment region --(?:[^!#$%&*+./<>?@\\\^|~=]|$) $                fill comment

add-highlighter shared/haskell/code/ regex (?<!')\b0x+[A-Fa-f0-9]+ 0:value
add-highlighter shared/haskell/code/ regex (?<!')\b\d+([.]\d+)? 0:value
add-highlighter shared/haskell/code/ regex (?<!')\b(import|hiding|qualified|module)(?!')\b 0:keyword
add-highlighter shared/haskell/code/ regex (?<!')\b(import)(?!')\b[^\n]+(?<!')\b(as)(?!')\b 2:keyword
add-highlighter shared/haskell/code/ regex (?<!')\b(class|data|default|deriving|infix|infixl|infixr|instance|module|newtype|pattern|type|where)(?!')\b 0:keyword
add-highlighter shared/haskell/code/ regex (?<!')\b(case|do|else|if|in|let|mdo|of|proc|rec|then)(?!')\b 0:attribute

# The complications below is because period has many uses:
# As function composition operator (possibly without spaces) like "." and "f.g"
# Hierarchical modules like "Data.Maybe"
# Qualified imports like "Data.Maybe.Just", "Data.Maybe.maybe", "Control.Applicative.<$>"
# Quantifier separator in "forall a . [a] -> [a]"
# Enum comprehensions like "[1..]" and "[a..b]" (making ".." and "Module..." illegal)

# matches uppercase identifiers:  Monad Control.Monad
# not non-space separated dot:    Just.const
add-highlighter shared/haskell/code/ regex \b([A-Z]['\w]*\.)*[A-Z]['\w]*(?!['\w])(?![.a-z]) 0:variable

# matches infix identifier: `mod` `Apa._T'M`
add-highlighter shared/haskell/code/ regex `\b([A-Z]['\w]*\.)*[\w]['\w]*` 0:operator
# matches imported operators: M.! M.. Control.Monad.>>
# not operator keywords:      M... M.->
add-highlighter shared/haskell/code/ regex \b[A-Z]['\w]*\.[~<=>|:!?/.@$*&#%+\^\-\\]+ 0:operator
# matches dot: .
# not possibly incomplete import:  a.
# not other operators:             !. .!
add-highlighter shared/haskell/code/ regex (?<![\w~<=>|:!?/.@$*&#%+\^\-\\])\.(?![~<=>|:!?/.@$*&#%+\^\-\\]) 0:operator
# matches other operators: ... > < <= ^ <*> <$> etc
# not dot: .
# not operator keywords:  @ .. -> :: ~
add-highlighter shared/haskell/code/ regex (?<![~<=>|:!?/.@$*&#%+\^\-\\])[~<=>|:!?/.@$*&#%+\^\-\\]+ 0:operator

# matches operator keywords: @ ->
add-highlighter shared/haskell/code/ regex (?<![~<=>|:!?/.@$*&#%+\^\-\\])(@|~|<-|->|=>|::|=|:|[|])(?![~<=>|:!?/.@$*&#%+\^\-\\]) 1:keyword
# matches: forall [..variables..] .
# not the variables
add-highlighter shared/haskell/code/ regex \b(forall)\b[^.\n]*?(\.) 1:keyword 2:keyword

# matches 'x' '\\' '\'' '\n' '\0'
# not incomplete literals: '\'
# not valid identifiers:   w' _'
add-highlighter shared/haskell/code/ regex \B'([^\\]|[\\]['"\w\d\\])' 0:string
# this has to come after operators so '-' etc is correct

# Commands
# ‾‾‾‾‾‾‾‾

# http://en.wikibooks.org/wiki/Haskell/Indentation

define-command -hidden haskell-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden haskell-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K--\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # align to first clause
        try %{ execute-keys -draft \; k x X s ^\h*(if|then|else)?\h*(([\w']+\h+)+=)?\h*(case\h+[\w']+\h+of|do|let|where)\h+\K.* <ret> s \A|.\z <ret> & }
        # filter previous line
        try %{ execute-keys -draft k : haskell-filter-around-selections <ret> }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ execute-keys -draft \; k x <a-k> ^\h*(if)|(case\h+[\w']+\h+of|do|let|where|[=(])$ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group haskell-highlight global WinSetOption filetype=haskell %{ add-highlighter window/haskell ref haskell }

hook global WinSetOption filetype=haskell %{
    set-option window extra_word_chars "'"
    hook window ModeChange insert:.* -group haskell-hooks  haskell-filter-around-selections
    hook window InsertChar \n -group haskell-indent haskell-indent-on-new-line
}

hook -group haskell-highlight global WinSetOption filetype=(?!haskell).* %{ remove-highlighter window/haskell }

hook global WinSetOption filetype=(?!haskell).* %{
    remove-hooks window haskell-indent
    remove-hooks window haskell-hooks
}
