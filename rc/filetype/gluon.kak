# http://gluon-lang.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](glu) %{
    set-option buffer filetype gluon
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=gluon %{
    require-module gluon

    set-option window extra_word_chars '_' "'"
    hook window ModeChange pop:insert:.* -group gluon-trim-indent gluon-trim-indent
    hook window InsertChar \n -group gluon-insert gluon-insert-on-new-line
    hook window InsertChar \n -group gluon-indent gluon-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window gluon-.+
    }
}

hook -group gluon-highlight global WinSetOption filetype=gluon %{
    add-highlighter window/gluon ref gluon
    hook -once -always window WinSetOption filetype=.* %{
        remove-highlighter window/gluon
    }
}


provide-module gluon %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/gluon regions
add-highlighter shared/gluon/code default-region group
add-highlighter shared/gluon/string region (?<!')" (?<!\\)(\\\\)*" fill string
add-highlighter shared/gluon/raw_string region -match-capture \br(#+)" \"(#+) fill string
add-highlighter shared/gluon/comment region /\* \*/ fill comment
add-highlighter shared/gluon/line_comment region // $ fill comment
add-highlighter shared/gluon/attribute region -recurse \[ '#\[' \] fill meta
# balance out bracket ]

# matches hexadecimal literals
add-highlighter shared/gluon/code/ regex \b0x+[A-Fa-f0-9]+ 0:value
# matches decimal and floating-point literals
add-highlighter shared/gluon/code/ regex \b\d+([.]\d+)? 0:value

# matches keywords
add-highlighter shared/gluon/code/ regex \
    (?<!')\b(type|if|then|else|match|with|let|rec|do|seq|in)\b(?!') 0:keyword

# matches macros
add-highlighter shared/gluon/code/ regex \b\w+! 0:meta

# matches uppercase identifiers: Monad Some
add-highlighter shared/gluon/code/ regex \b[A-Z][\w']* 0:variable

# matches operators: ... > < <= ^ <*> <$> etc
# matches dot: .
# matches keywords:  @ : ->
add-highlighter shared/gluon/code/ regex (?<![~<=>|:!?/.@$*&#%+\^\-\\])[~<=>|:!?/.@$*&#%+\^\-\\]+ 0:operator

# matches 'x' '\\' '\'' '\n' '\0'
# not incomplete literals: '\'
add-highlighter shared/gluon/code/ regex \B'([^\\]|[\\]['"\w\d\\])' 0:string
# this has to come after operators so '-' etc is correct

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden gluon-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden gluon-insert-on-new-line %~
    evaluate-commands -draft -itersel %_
        # copy // and /// comments prefix and following white spaces
        try %{ execute-keys -draft k x s ^\h*\K///?\h* <ret> y gh j P }
    _
~

define-command -hidden gluon-indent-on-new-line %~
    evaluate-commands -draft -itersel %_
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : gluon-trim-indent <ret> }
        # indent after lines ending with (open) braces, =, ->, condition, rec,
        # or in
        try %{ execute-keys -draft \; k x <a-k> (\(|\{|\[|=|->|\b(?:then|else|rec|in))$ <ret> j <a-gt> }
        # deindent closing brace(s) when after cursor
        try %< execute-keys -draft x <a-k> ^\h*[})\]] <ret> gh / \})\]] <ret> m <a-S> 1<a-&> >
    _
~

§
