# https://dhall-lang.org
#                       

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](dhall) %{
    set-option buffer filetype dhall
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=dhall %{
    require-module dhall

    hook window ModeChange pop:insert:.* -group dhall-trim-indent dhall-trim-indent
    hook window InsertChar \n -group dhall-insert dhall-insert-on-new-line
    hook window InsertChar \n -group dhall-indent dhall-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window dhall-.+ }
}

hook -group dhall-highlight global WinSetOption filetype=dhall %{
    add-highlighter window/dhall ref dhall
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/dhall }
}


provide-module dhall %[
    
# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/dhall regions
add-highlighter shared/dhall/code default-region group
add-highlighter shared/dhall/string  region          '"'          (?<!\\)(\\\\)*" fill string
add-highlighter shared/dhall/comment region -recurse \{- \{-      -\}             fill comment
add-highlighter shared/dhall/line_comment region -- $ fill comment

# Matches multi-line string literals
add-highlighter shared/dhall/multiline_string region \Q''\E$ [^']''[^'] fill string

# Matches quoted labels
add-highlighter shared/dhall/quoted_label region ` ` fill normal

# Matches built-in types
add-highlighter shared/dhall/code/ regex \b(Location|Sort|Kind|Type|Text|Bool|Natural|Integer|Double|List|Optional|\{\})\b 0:type

# Matches built-in keywords
add-highlighter shared/dhall/code/ regex \b(if|then|else|let|in|using|missing|as|merge|toMap)\b 0:keyword

# Matches bulit-in values
add-highlighter shared/dhall/code/ regex \b(True|False|Some|None|-?Infinity|\{=\}|NaN)\b 0:value

# Matches built-in operators
add-highlighter shared/dhall/code/ regex (,|:|\|\||&&|==|!=|=|\+|\*|\+\+|#|⩓|//\\\\|→|->|\?|λ|\\|\^|⫽|//|\[|\]|\{|\}) 0:operator

# Matches built-in functions
add-highlighter shared/dhall/code/ regex \b(Natural-fold|Natural-build|Natural-isZero|Natural-even|Natural-odd|Natural-toInteger|Natural-show|Integer-toDouble|Integer-show|Natural-subtract|Double-show|List-build|List-fold|List-length|List-head|List-last|List-indexed|List-reverse|Optional-fold|Optional-build|Text-show)\b 0:keyword

# Matches http[s] imports
add-highlighter shared/dhall/code/ regex \b(http[s]://\S+)\b(\s+sha256:[a-f0-9]{64}\b)? 0:meta

# Matches local imports
add-highlighter shared/dhall/code/ regex (~|\.|\.\.|/)\S+ 0:meta

# Matches number (natural, integer, double) literals
add-highlighter shared/dhall/code/ regex \b(\+|-)?\d+(\.\d+)?(e(\+|-)?\d+)?\b 0:value

# Matches union syntax
add-highlighter shared/dhall/union region -recurse < < > group
add-highlighter shared/dhall/union/sep regex (<|\|)\s*((?:_|[A-Z])(?:[a-zA-Z0-9-/_]*))\s*(?:(:)([^|>]*))? 1:operator 2:attribute 3:operator 4:type
add-highlighter shared/dhall/union/end regex > 0:operator

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden dhall-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden dhall-insert-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy -- comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K--\h* <ret> y gh j P }
    }
}
define-command -hidden dhall-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : dhall-trim-indent <ret> }
        # indent after lines ending with let, : or =
        try %{ execute-keys -draft \; k x <a-k> (\blet|:|=)$ <ret> j <a-gt> }
    }
}

]
