# http://elm-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](elm) %{
    set-option buffer filetype elm
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/elm regions
add-highlighter shared/elm/code default-region group
add-highlighter shared/elm/string        region '"'     (?<!\\)(\\\\)*"       fill string
add-highlighter shared/elm/line_comment  region (--) $                        fill comment
add-highlighter shared/elm/comment       region -recurse \{- \{-   -\}        fill comment

add-highlighter shared/elm/code/ regex \b(import|exposing|as|module|where)\b 0:meta
add-highlighter shared/elm/code/ regex \b(True|False)\b 0:value
add-highlighter shared/elm/code/ regex \b(if|then|else|case|of|let|in|type|port|alias)\b 0:keyword
add-highlighter shared/elm/code/ regex \b(Array|Bool|Char|Float|Int|String)\b 0:type

# Commands
# ‾‾‾‾‾‾‾‾

# http://elm-lang.org/docs/style-guide

define-command -hidden elm-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden elm-indent-after "
 execute-keys -draft \\; k x <a-k> ^\\h*(if)|(case\\h+[\\w']+\\h+of|let|in|\\{\\h+\\w+|\\w+\\h+->|[=(])$ <ret> j <a-gt>
"

define-command -hidden elm-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K--\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # align to first clause
        try %{ execute-keys -draft \; k x X s ^\h*(if|then|else)?\h*(([\w']+\h+)+=)?\h*(case\h+[\w']+\h+of|let)\h+\K.* <ret> s \A|.\z <ret> & }
        # filter previous line
        try %{ execute-keys -draft k : elm-filter-around-selections <ret> }
        # indent after lines beginning with condition or ending with expression or =(
        try %{ elm-indent-after }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group elm-highlight global WinSetOption filetype=elm %{ add-highlighter window/elm ref elm }

hook global WinSetOption filetype=elm %{
    hook window ModeChange insert:.* -group elm-hooks  elm-filter-around-selections
    hook window InsertChar \n -group elm-indent elm-indent-on-new-line
}

hook -group elm-highlight global WinSetOption filetype=(?!elm).* %{ remove-highlighter window/elm }

hook global WinSetOption filetype=(?!elm).* %{
    remove-hooks window elm-indent
    remove-hooks window elm-hooks
}
