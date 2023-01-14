
# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(apl|aplf|aplo|apln|aplc|apli|dyalog) %{
    set-option buffer filetype apl

    set-option buffer matching_pairs ( ) { } [ ]
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=apl %|
    require-module apl

    hook window InsertChar \n -group apl-indent apl-indent-on-new-line
    hook window InsertChar [}⟩\]] -group apl-indent apl-indent-on-closing

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window apl-.+ }
|

hook -group apl-highlight global WinSetOption filetype=apl %{
    add-highlighter window/apl ref apl
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/apl }
}

provide-module apl %¹

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/apl regions
add-highlighter shared/apl/code default-region group
add-highlighter shared/apl/comment region "⍝" "$" fill comment
add-highlighter shared/apl/string  region "'" "'" fill string

add-highlighter shared/apl/code/ regex "[{}]" 0:meta
add-highlighter shared/apl/code/ regex "⋄" 0:meta
add-highlighter shared/apl/code/ regex "[\[\]]" 0:magenta
add-highlighter shared/apl/code/ regex "[()]" 0:bright-black
add-highlighter shared/apl/code/ regex "[:;?]" 0:bright-black
add-highlighter shared/apl/code/ regex "[←→]" 0:normal
# Number
add-highlighter shared/apl/code/ regex "¯?[0-9][¯0-9A-Za-z]*(?:\.[¯0-9Ee][¯0-9A-Za-z]*)*|¯?\.[0-9Ee][¯0-9A-Za-z]*" 0:value
add-highlighter shared/apl/code/ regex "\." 0:normal
add-highlighter shared/apl/code/ regex "[⍺⍶⍵⍹χ∇]" 0:blue # arguments
# function
add-highlighter shared/apl/code/ regex "[+\-×÷⌈⌊∣|⍳?*⍟○!⌹<≤=>≥≠≡≢∊⍷∪∩~∨∧⍱⍲⍴,⍪⌽⊖⍉↑↓⊂⊃⌷⍋⍒⊤⊥⍕⍎⊣⊢⍁⍂≈⌸⍯↗⊆⊇⍸√⌾…⍮]" 0:green  # function
# operator
add-highlighter shared/apl/code/ regex "[\.\\/⌿⍀¨⍣⍨⍠⍤∘⌸&⌶@⌺⍥⍛⍢]" 0:magenta
# system
add-highlighter shared/apl/code/ regex "⎕[A-Z_a-zÀ-ÖØ-Ýßà-öø-üþ∆⍙Ⓐ-Ⓩ][A-Z_a-zÀ-ÖØ-Ýßà-öø-üþ∆⍙Ⓐ-Ⓩ¯0-9]*" 0:yellow 
add-highlighter shared/apl/code/ regex "\n^\s*\n(\n\t[A-Z_a-zÀ-ÖØ-Ýßà-öø-üþ∆⍙Ⓐ-Ⓩ]\n\t[A-Z_a-zÀ-ÖØ-Ýßà-öø-üþ∆⍙Ⓐ-Ⓩ¯0-9]*\n)\n(:)" 0:meta

declare-user-mode apl
# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden apl-indent-on-new-line %`
    evaluate-commands -draft -itersel %_
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # copy # comments prefix
        try %{ execute-keys -draft <semicolon><c-s>k<a-x> s ^\h*\K#+\h* <ret> y<c-o>P<esc> }
        # indent after lines ending with { [
        try %( execute-keys -draft k<a-x> <a-k> [{\[]\h*$ <ret> j<a-gt> )
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft k<a-x> s \h+$ <ret>d }
     _
`

define-command -hidden apl-indent-on-closing %`
    evaluate-commands -draft -itersel %_
        # align to opening bracket
        try %( execute-keys -draft <a-h> <a-k> ^\h*[}\]]$ <ret> h m <a-S> 1<a-&> )
    _
`

¹
