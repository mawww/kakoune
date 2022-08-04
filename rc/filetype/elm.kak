# http://elm-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](elm) %{
    set-option buffer filetype elm
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=elm %{
    require-module elm

    hook window ModeChange pop:insert:.* -group elm-trim-indent elm-trim-indent
    hook window InsertChar \n -group elm-insert elm-insert-on-new-line
    hook window InsertChar \n -group elm-indent elm-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window elm-.+ }
}

hook -group elm-highlight global WinSetOption filetype=elm %{
    add-highlighter window/elm ref elm
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/elm }
}


provide-module elm %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/elm               regions
add-highlighter shared/elm/code          default-region group
add-highlighter shared/elm/multiline_string region '"""' '"""' fill string
add-highlighter shared/elm/string        region         '"'     (?<!\\)(\\\\)*"       fill string
add-highlighter shared/elm/line_comment  region         (--) $                        fill comment
add-highlighter shared/elm/comment       region         -recurse \{- \{-   -\}        fill comment

add-highlighter shared/elm/code/ regex \b[A-Z]\w*\b                                                        0:type
add-highlighter shared/elm/code/ regex \b[a-z]\w*\b                                                        0:variable
add-highlighter shared/elm/code/ regex ^[a-z]\w*\b                                                         0:function
add-highlighter shared/elm/code/ regex "-?\b[0-9]*\.?[0-9]+"                                               0:value
add-highlighter shared/elm/code/ regex \B[-+<>!@#$%^&*=:/\\|]+\B                                           0:operator
add-highlighter shared/elm/code/ regex \b(import|exposing|as|module|port)\b                                0:meta
add-highlighter shared/elm/code/ regex \b(type|alias|if|then|else|case|of|let|in|infix|_)\b)               0:keyword
add-highlighter shared/elm/code/ regex (?<![-+<>!@#$%^&*=:/\\|])(->|:|=|\|)(?![-+<>!@#$%^&*=:/\\|])        0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

# http://elm-lang.org/docs/style-guide

define-command -hidden elm-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden elm-indent-after "
 execute-keys -draft <semicolon> k x <a-k> ^\\h*if|[=(]$|\\b(case\\h+[\\w']+\\h+of|let|in)$|(\\{\\h+\\w+|\\w+\\h+->)$ <ret> j <a-gt>
"

define-command -hidden elm-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K--\h* <ret> y gh j P }
    }
}

define-command -hidden elm-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # align to first clause
        try %{ execute-keys -draft <semicolon> k x X s ^\h*(if|then|else)?\h*(([\w']+\h+)+=)?\h*(case\h+[\w']+\h+of|let)\h+\K.* <ret> s \A|.\z <ret> & }
        # filter previous line
        try %{ execute-keys -draft k : elm-trim-indent <ret> }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ elm-indent-after }
    }
}

]
