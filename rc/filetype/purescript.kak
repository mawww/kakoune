# http://purescript.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
# Syntax reference
# https://github.com/purescript/documentation/blob/master/language/Syntax.md

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](purs) %{
    set-option buffer filetype purescript
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=purescript %{
    require-module purescript

    set-option buffer extra_word_chars '_' "'"
    hook window ModeChange pop:insert:.* -group purescript-trim-indent purescript-trim-indent
    hook window InsertChar \n -group purescript-insert purescript-insert-on-new-line
    hook window InsertChar \n -group purescript-indent purescript-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window purescript-.+ }
}

hook -group purescript-highlight global WinSetOption filetype=purescript %{
    add-highlighter window/purescript ref purescript
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/purescript }
}


provide-module purescript %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/purescript regions
add-highlighter shared/purescript/code default-region group
add-highlighter shared/purescript/string       region (?<!'\\)(?<!')"                 (?<!\\)(\\\\)*"  fill string
add-highlighter shared/purescript/comment      region -recurse \{- \{-                  -\}            fill comment
add-highlighter shared/purescript/line_comment region --(?:[^!#$%&*+./<>?@\\\^|~=]|$) $                fill comment

add-highlighter shared/purescript/code/ regex (?<!')\b0x+[A-Fa-f0-9]+ 0:value
add-highlighter shared/purescript/code/ regex (?<!')\b\d+([.]\d+)? 0:value
add-highlighter shared/purescript/code/ regex (?<!')\b(import|hiding|module)(?!')\b 0:keyword
add-highlighter shared/purescript/code/ regex (?<!')\b(import)(?!')\b[^\n]+(?<!')\b(as)(?!')\b 2:keyword
add-highlighter shared/purescript/code/ regex (?<!')\b(class|data|default|derive|infix|infixl|infixr|instance|module|newtype|pattern|type|where)(?!')\b 0:keyword
add-highlighter shared/purescript/code/ regex (?<!')\b(case|do|else|if|in|let|mdo|of|proc|rec|then)(?!')\b 0:attribute
add-highlighter shared/purescript/code/ regex (?<!')\b(type|data)\b\s+(\bfamily\b)?(?!') 0:keyword

# The complications below is because period has many uses:
# As function composition operator (possibly without spaces) like "." and "f.g"
# Hierarchical modules like "Data.Maybe"
# Qualified imports like "Data.Maybe.Just", "Data.Maybe.maybe", "Control.Applicative.<$>"
# Quantifier separator in "forall a . [a] -> [a]"
# Enum comprehensions like "[1..]" and "[a..b]" (making ".." and "Module..." illegal)

# matches uppercase identifiers:  Monad Control.Monad
# not non-space separated dot:    Just.const
add-highlighter shared/purescript/code/ regex \b([A-Z]['\w]*\.)*[A-Z]['\w]*(?!['\w])(?![.a-z]) 0:variable

# matches infix identifier: `mod` `Apa._T'M`
add-highlighter shared/purescript/code/ regex `\b([A-Z]['\w]*\.)*[\w]['\w]*` 0:operator
# matches imported operators: M.! M.. Control.Monad.>>
# not operator keywords:      M... M.->
add-highlighter shared/purescript/code/ regex \b[A-Z]['\w]*\.[~<=>|:!?/.@$*&#%+\^\-\\]+ 0:operator
# matches dot: .
# not possibly incomplete import:  a.
# not other operators:             !. .!
add-highlighter shared/purescript/code/ regex (?<![\w~<=>|:!?/.@$*&#%+\^\-\\])\.(?![~<=>|:!?/.@$*&#%+\^\-\\]) 0:operator
# matches other operators: ... > < <= ^ <*> <$> etc
# not dot: .
# not operator keywords:  @ .. -> :: ~
add-highlighter shared/purescript/code/ regex (?<![~<=>|:!?/.@$*&#%+\^\-\\])[~<=>|:!?/.@$*&#%+\^\-\\]+ 0:operator

# matches operator keywords: @ ->
add-highlighter shared/purescript/code/ regex (?<![~<=>|:!?/.@$*&#%+\^\-\\])(@|~|<-|->|=>|::|=|:|[|])(?![~<=>|:!?/.@$*&#%+\^\-\\]) 1:keyword
# matches: forall [..variables..] .
# not the variables
add-highlighter shared/purescript/code/ regex \b(forall|∀)\b[^.\n]*?(\.) 1:keyword 2:keyword

# matches 'x' '\\' '\'' '\n' '\0'
# not incomplete literals: '\'
# not valid identifiers:   w' _'
add-highlighter shared/purescript/code/ regex \B'([^\\]|[\\]['"\w\d\\])' 0:string
# this has to come after operators so '-' etc is correct

# matches function names in type signatures
add-highlighter shared/purescript/code/ regex ^\s*(?:where\s+|let\s+|default\s+)?([_a-z]['\w]*#?(?:,\s*[_a-z]['\w]*#?)*)\s+::\s 1:meta

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden purescript-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden purescript-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K--\h* <ret> y gh j P }
    }
}

define-command -hidden purescript-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # align to first clause
        try %{ execute-keys -draft <semicolon> k x X s ^\h*(if|then|else)?\h*(([\w']+\h+)+=)?\h*(case\h+[\w']+\h+of|do|let|where)\h+\K.* <ret> s \A|.\z <ret> & }
        # filter previous line
        try %{ execute-keys -draft k : purescript-trim-indent <ret> }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ execute-keys -draft <semicolon> k x <a-k> ^\h*if|[=(]$|\b(case\h+[\w']+\h+of|do|let|where)$ <ret> j <a-gt> }
    }
}

§
