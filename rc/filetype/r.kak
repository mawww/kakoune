# http://kakoune.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate (.*/)?(\.Rprofile|.*\.[rR]) %{
    set-option buffer filetype r
}

provide-module r %§

# Highlighters & Completion
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/r regions
add-highlighter shared/r/code default-region group
add-highlighter shared/r/double_string region '"'   (?<!\\)(\\\\)*"  fill string
add-highlighter shared/r/single_string region "'"   (?<!\\)(\\\\)*'  fill string
add-highlighter shared/r/comment       region '#'   '$'              fill comment
add-highlighter shared/r/identifier    region '`'   (?<!\\)(\\\\)*`  fill attribute

# see base::NumericConstants
add-highlighter shared/r/code/ regex '(?i)\b(?<![.\d])\d+(\.\d*)?(e[-+]?\d+)?(?I)[iL]?(?![\d.e\w])' 0:value
add-highlighter shared/r/code/ regex '(?i)(?<![.\d\w])\.\d+(e[-+]?\d+)?(?I)[iL]?(?![\d.e])' 0:value
add-highlighter shared/r/code/ regex '(?i)\b(?<![.\d])0x[0-9a-f]+?(p[-+]?\d+)?(?I)[iL]?(?![\d.e\w])' 0:value

evaluate-commands %sh{
    # see base::Reserved
    values="TRUE|FALSE|NULL|Inf|NaN|NA|NA_integer_|NA_real_|NA_complex_|NA_character_|\.{3}|\.{2}\d+|"
    keywords="if|else|repeat|while|function|for|in|next|break"

    # see base::Ops and methods::Ops
    math_functions="abs|sign|sqrt|floor|ceiling|trunc|round|signif|exp|log|expm1|log1p|cos|sin|tan|cospi|sinpi|tanpi|acos|asin|atan|cosh|sinh|tanh|acosh|asinh|atanh|lgamma|gamma|digamma|trigamma"
    summary_functions="all|any|sum|prod|min|max|range"
    complex_functions="Arg|Conj|Im|Mod|Re"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=python %{
        set-option window static_words ${values} ${keywords} ${math_functions} ${summary_functions} ${complex_functions}
    }" | tr '|' ' '

    printf %s "
        add-highlighter shared/r/code/ regex '\b(${values})\b' 0:value
        add-highlighter shared/r/code/ regex '\b(${keywords})\b' 0:keyword
        add-highlighter shared/r/code/ regex '\b(${math_functions}|${summary_functions}|${complex_functions})\b' 0:function
    "
}

# see base::Syntax
add-highlighter shared/r/code/ regex (?<=[\w\s\d'"_)])(\$|@|\^|-|\+|%[^%^\n]+%|\*|/|<|>|<=|>=|!|&{1,2}|\|{1,2}|~|\?|:{1,3}|\[{1,2}|\]{1,2}|={1,2}|<{1,2}-|->{1,2}|!=|%%) 0:operator


# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden r-trim-indent %{
    # remove the line if it's empty when leaving the insert mode
    try %{ execute-keys -draft x 1s^(\h+)$<ret> d }
}

define-command -hidden r-indent-on-newline %< evaluate-commands -draft -itersel %<
    execute-keys <semicolon>
    try %<
        # if previous line closed a paren (possibly followed by words and a comment),
        # copy indent of the opening paren line
        execute-keys -draft kx 1s(\))(\h+\w+)*\h*(\;\h*)?(?:#[^\n]+)?\n\z<ret> m<a-semicolon>J <a-S> 1<a-&>
    > catch %<
        # else indent new lines with the same level as the previous one
        execute-keys -draft K <a-&>
    >
    # remove previous empty lines resulting from the automatic indent
    try %< execute-keys -draft k x <a-k>^\h+$<ret> Hd >
    # indent after an opening brace or parenthesis at end of line
    try %< execute-keys -draft k x s[{(]\h*$<ret> j <a-gt> >
    # indent after a statement not followed by an opening brace
    try %< execute-keys -draft k x s\)\h*(?:#[^\n]+)?\n\z<ret> \
                               <a-semicolon>mB <a-k>\A\b(if|for|while)\b<ret> <a-semicolon>j <a-gt> >
    try %< execute-keys -draft k x s \belse\b\h*(?:#[^\n]+)?\n\z<ret> \
                               j <a-gt> >
    # deindent after a single line statement end
    try %< execute-keys -draft K x <a-k>\;\h*(#[^\n]+)?$<ret> \
                               K x s\)(\h+\w+)*\h*(#[^\n]+)?\n([^\n]*\n){2}\z<ret> \
                               MB <a-k>\A\b(if|for|while)\b<ret> <a-S>1<a-&> >
    try %< execute-keys -draft K x <a-k>\;\h*(#[^\n]+)?$<ret> \
                               K x s \belse\b\h*(?:#[^\n]+)?\n([^\n]*\n){2}\z<ret> \
                               <a-S>1<a-&> >
    # align to the opening parenthesis or opening brace (whichever is first)
    # on a previous line if its followed by text on the same line
    try %< evaluate-commands -draft %<
        # Go to opening parenthesis and opening brace, then select the most nested one
        try %< execute-keys [c [({],[)}] <ret> >
        # Validate selection and get first and last char
        execute-keys <a-k>\A[{(](\h*\S+)+\n<ret> <a-K>"(([^"]*"){2})*<ret> <a-K>'(([^']*'){2})*<ret> <a-:><a-semicolon>L <a-S>
        # Remove possibly incorrect indent from new line which was copied from previous line
        try %< execute-keys -draft , <a-h> s\h+<ret> d >
        # Now indent and align that new line with the opening parenthesis/brace
        execute-keys 1<a-&> &
     > >
> >

define-command -hidden r-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> <a-S> 1<a-&> ]
]

define-command -hidden r-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[
        # in case open curly brace follows a closing paren, align indent with opening paren
        execute-keys -itersel -draft <a-h><a-:><a-k>^\h+\}$<ret>hm <a-F>)M <a-k> \A\(.*\)\h\{.*\}\z <ret> <a-S>1<a-&>
    ] catch %[
        # otherwise align with open curly brace
        execute-keys -itersel -draft <a-h><a-:><a-k>^\h+\}$<ret>hm<a-S>1<a-&>
    ] catch %[]
]

define-command -hidden r-insert-on-newline %[ evaluate-commands -itersel -draft %[
    execute-keys <semicolon>
    try %[
        evaluate-commands -draft -save-regs '/"' %[
            # copy the commenting prefix
            execute-keys -save-regs '' k x1s^\h*(#+\h*)<ret> y
            try %[
                # if the previous comment isn't empty, create a new one
                execute-keys x<a-K>^\h*#+\h*$<ret> jxs^\h*<ret>P
            ] catch %[
                # if there is no text in the previous comment, remove it completely
                execute-keys d
            ]
        ]
    ]
] ]

§

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group r-highlight global WinSetOption filetype=r %{
    require-module r
    add-highlighter window/r ref r
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/r }
}

hook global WinSetOption filetype=r %~
    require-module r
    hook window ModeChange pop:insert:.* r-trim-indent
    hook window InsertChar \n        r-insert-on-newline
    hook window InsertChar \n        r-indent-on-newline
    hook window InsertChar \{        r-indent-on-opening-curly-brace
    hook window InsertChar \}        r-indent-on-closing-curly-brace

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window r-.+ }
~
