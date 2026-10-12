# https://www.nushell.sh
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
# Based on:
# - https://raw.githubusercontent.com/shikijs/textmate-grammars-themes/refs/heads/main/packages/tm-grammars/grammars/nushell.json
# - https://github.com/nushell/vscode-nushell-lang/blob/main/syntaxes/nushell.tmLanguage.json
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](nu) %{
    set-option buffer filetype nushell
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=nushell %<
    require-module nushell

    hook window ModeChange pop:insert:.* -group nushell-trim-indent nushell-trim-indent
    hook window InsertChar \n            -group nushell-insert      nushell-insert-on-new-line
    hook window InsertChar \n            -group nushell-indent      nushell-indent-on-new-line
    hook window InsertChar [\]})]        -group nushell-indent      nushell-deindent-on-closing

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window nushell-.+ }
>

hook -group nushell-highlight global WinSetOption filetype=nushell %{
    add-highlighter window/nushell ref nushell
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/nushell }
}


provide-module nushell %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/nushell regions
add-highlighter shared/nushell/code default-region group

add-highlighter shared/nushell/interp_double region %{\$"} %{(?<!\\)(\\\\)*"} group
add-highlighter shared/nushell/interp_single region %{\$'} %{'} group

add-highlighter shared/nushell/interp_double/ fill string
add-highlighter shared/nushell/interp_double/ regex \$\w+ 0:variable
add-highlighter shared/nushell/interp_double/ regex %{\\(?:["\\/bfnrt()]|u[0-9a-fA-F]{4})} 0:meta

add-highlighter shared/nushell/interp_single/ fill string
add-highlighter shared/nushell/interp_single/ regex \$\w+ 0:variable

# String regions
add-highlighter shared/nushell/double_string   region %{"} %{(?<!\\)(\\\\)*"} group
add-highlighter shared/nushell/single_string   region "'" "'" fill string
add-highlighter shared/nushell/backtick_string region '`' '`' fill string
add-highlighter shared/nushell/raw_string      region -match-capture %{r(#+)'} %{'(#+)} fill string

add-highlighter shared/nushell/binary_blob     region %{\b0x\[} %{\]} fill value

add-highlighter shared/nushell/comment region '#' '$' fill comment

add-highlighter shared/nushell/double_string/ fill string
add-highlighter shared/nushell/double_string/ regex %{\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4})} 0:meta

# Variable declarations - highlight keyword and name separately
# let/mut/const name [: type] =
add-highlighter shared/nushell/code/ regex "(let|mut|(?:export\h+)?const)\h+(\w+)" 1:keyword 2:variable
# alias name =
add-highlighter shared/nushell/code/ regex "((?:export\h+)?alias)\h+([\w-]+)" 1:keyword 2:function
# def / export def name
add-highlighter shared/nushell/code/ regex "((?:export\h+)?def)\h+([\w-]+)" 1:keyword 2:function
# extern / export extern name
add-highlighter shared/nushell/code/ regex "((?:export\h+)?extern)\h+([\w-]+)" 1:keyword 2:function
# module / export module name
add-highlighter shared/nushell/code/ regex "((?:export\h+)?module)\h+([\w-]+)" 1:keyword 2:module
# use / export use name
add-highlighter shared/nushell/code/ regex "((?:export\h+)?use)\h+([\w-]+)" 1:keyword 2:module
# for $var in
add-highlighter shared/nushell/code/ regex "\b(for)\h+(\$?\w+)\h+(in)\b" 1:keyword 2:variable 3:keyword

# Remaining keywords not covered by the declaration rules above
add-highlighter shared/nushell/code/ regex \b(break|continue|else|if|loop|return|try|while|match|catch|mut|source|overlay)\b 0:keyword

# Variables with optional field access: $var  $var.field  $var.field.sub
# Use bare (unquoted) form — %{} quoting does not work reliably for code-region regex rules
add-highlighter shared/nushell/code/ regex \$[\w][\w-]*(?:\.[\w-]+)* 0:variable

# Built-in language variables
add-highlighter shared/nushell/code/ regex \$(?:nu|env)\b 0:builtin

# Operators: word-form (space or paren delimited)
add-highlighter shared/nushell/code/ regex "(?<=[ (])(?:mod|in|not-(?:in|like|has)|not|and|or|xor|bit-(?:or|and|xor|shl|shr)|starts-with|ends-with|like|has)(?=[ )|])" 0:operator

# Pipe
add-highlighter shared/nushell/code/ regex \| 0:operator

# Spread operator
add-highlighter shared/nushell/code/ regex \.\.\.(?=\S) 0:operator

# Range operator
add-highlighter shared/nushell/code/ regex \.\.<? 0:operator

# Arithmetic and comparison operators (nushell requires space-delimited operators)
add-highlighter shared/nushell/code/ regex "(?<= )(?:\*\*|//|[-+*/%]|!=|[<>]=?|==|[!=]~|\+\+=?)(?= )" 0:operator

# Type annotation arrow
add-highlighter shared/nushell/code/ regex -> 0:operator

# Datetime literals  2024-01-15T12:00:00Z
add-highlighter shared/nushell/code/ regex \b\d{4}-\d{2}-\d{2}(?:T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:[+-]\d{2}:?\d{2}|Z)?)?\b 0:value

# Numeric literals: optional sign, _ separators, and size/duration units
add-highlighter shared/nushell/code/ regex "(?<![[\w-])[-+]?\d[\d_]*(?:\.\d[\d_]*)?(?:ns|us|ms|sec|min|hr|day|wk|kb|mb|gb|tb|pb|eb|kib|mib|gib|tib|pib|eib|b)?\b" 0:value
add-highlighter shared/nushell/code/ regex \b0x[0-9a-fA-F][0-9a-fA-F_]*\b 0:value
add-highlighter shared/nushell/code/ regex \b0b[01][01_]*\b 0:value
add-highlighter shared/nushell/code/ regex \b0o[0-7][0-7_]*\b 0:value

# Boolean and null constants
add-highlighter shared/nushell/code/ regex \b(true|false|null)\b 0:value

# Flags / named parameters  --flag  -f
add-highlighter shared/nushell/code/ regex (?<=\s)--?[\w][-\w]* 0:attribute

# Type annotations after ':' in parameter lists and signatures
add-highlighter shared/nushell/code/ regex ":\s*(any|int|float|string|bool|list|record|table|block|closure|nothing|error|binary|duration|filesize|date|range|glob|cell-path)\b" 1:type

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden nushell-trim-indent %{
    evaluate-commands -no-hooks -draft -itersel %{
        try %{ execute-keys -draft x 1s^(\h+)$<ret> d }
    }
}

define-command -hidden nushell-insert-on-new-line %{
    evaluate-commands -no-hooks -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*#\h* <ret> y jgh P }
    }
}

define-command -hidden nushell-indent-on-new-line %<
    evaluate-commands -no-hooks -draft -itersel %<
        # preserve previous line indent
        try %< execute-keys -draft <semicolon> K <a-&> >
        # cleanup trailing whitespace from previous line
        try %< execute-keys -draft k x s \h+$ <ret> d >
        # indent after opening {  [  (  or pipe |
        try %< execute-keys -draft k x <a-k> [[{(|]\h*$ <ret> j <a-gt> >
    >
>

define-command -hidden nushell-deindent-on-closing %{
    evaluate-commands -no-hooks -draft -itersel %{
        # deindent when a lone closing bracket is typed
        try %{ execute-keys -draft x <a-k> ^\h*[^\h]$ <ret> <a-lt> }
    }
}

§

# Alias module so that ```nu fenced blocks in Markdown also get Nushell highlighting
provide-module nu %{
    require-module nushell
    add-highlighter shared/nu ref nushell
}
