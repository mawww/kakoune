# https://developer.apple.com/library/archive/documentation/AppleScript/Conceptual/AppleScriptLangGuide/
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(applescript) %{
    set-option buffer filetype applescript
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=applescript %{
    require-module applescript

    hook window ModeChange pop:insert:.* -group applescript-trim-indent applescript-trim-indent
    hook window InsertChar \n -group applescript-insert applescript-insert-on-new-line
    hook window InsertChar \n -group applescript-indent applescript-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window applescript-.+ }
}

hook -group applescript-highlight global WinSetOption filetype=applescript %{
    add-highlighter window/applescript ref applescript
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/applescript }
}

provide-module applescript %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/applescript regions
add-highlighter shared/applescript/code default-region group

# Block comments: (* ... *)
add-highlighter shared/applescript/block_comment region -recurse \(\* \(\* \*\) fill comment

# Line comments: -- ...
add-highlighter shared/applescript/line_comment region -- $ fill comment

# Strings
add-highlighter shared/applescript/string region '"' '(?<!\\)(\\\\)*"' fill string

# Control flow
add-highlighter shared/applescript/code/ regex \
    \b(if|then|else|end|repeat|while|until|with|times|exit|try|considering|ignoring|using|from|terms)\b \
    0:keyword

# Declaration and scope
add-highlighter shared/applescript/code/ regex \
    \b(on|to|handler|script|property|global|local|return|error|continue|tell|application)\b \
    0:keyword

# Commands
add-highlighter shared/applescript/code/ regex \
    \b(set|get|copy|do|log|run|launch|quit|open|close|save|activate|delay|beep|say|shell)\b \
    0:keyword

# Reference and positional terms
add-highlighter shared/applescript/code/ regex \
    \b(of|in|into|through|thru|by|before|after|beginning|front|back|behind|above|below|at|between|out)\b \
    0:keyword

# Reference form selectors
add-highlighter shared/applescript/code/ regex \
    \b(every|some|any|first|second|third|fourth|fifth|sixth|seventh|eighth|ninth|tenth|last|middle|whose|where|its|it|me|my|result)\b \
    0:keyword

# Operators
add-highlighter shared/applescript/code/ regex \
    \b(and|or|not|div|mod|as|is|equal|equals|contains|ref)\b \
    0:operator

add-highlighter shared/applescript/code/ regex [&+\-*^/] 0:operator
add-highlighter shared/applescript/code/ regex [=<>] 0:operator

# Line continuation character ¬
add-highlighter shared/applescript/code/ regex ¬ 0:meta

# Boolean and null constants
add-highlighter shared/applescript/code/ regex \b(true|false|missing)\b 0:value

# Numeric literals
add-highlighter shared/applescript/code/ regex \b[0-9]+(?:\.[0-9]+)?(?:E[+-]?[0-9]+)?\b 0:value

# Coercible types
add-highlighter shared/applescript/code/ regex \
    \b(string|text|integer|real|boolean|list|record|date|alias|file|reference|number|class|data|script|handler|type)\b \
    0:type

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden applescript-trim-indent %<
    evaluate-commands -draft -itersel %<
        try %< execute-keys -draft x 1s^(\h+)$<ret> d >
    >
>

define-command -hidden applescript-insert-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy '--' comment prefix and following whitespace
        try %< execute-keys -draft k x s ^\h*--\h* <ret> y jgh P >
    >
>

define-command -hidden applescript-indent-on-new-line %<
    evaluate-commands -no-hooks -draft -itersel %<
        # preserve previous line indent
        try %< execute-keys -draft <semicolon> K <a-&> >
        try %<
            # only if we didn't copy a comment
            execute-keys -draft x <a-K> ^\h*-- <ret>
            # indent after lines starting with a block-opening keyword
            try %< execute-keys -draft k x <a-k> ^\h*(tell|repeat|on|to|try|considering|ignoring|script)\b <ret> j <a-gt> >
            # indent after lines ending with 'then' (if ... then)
            try %< execute-keys -draft k x <a-k> \bthen\h*$ <ret> j <a-gt> >
        >
        # cleanup trailing whitespace from previous line
        try %< execute-keys -draft k : applescript-trim-indent <ret> >
    >
>

§
