# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.][cm]?(js)x? %{
    set-option buffer filetype javascript
}

hook global BufCreate .*[.][cm]?(ts)x? %{
    set-option buffer filetype typescript
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=(javascript|typescript) %{
    require-module javascript

    hook window ModeChange pop:insert:.* -group "%val{hook_param_capture_1}-trim-indent" javascript-trim-indent
    hook window InsertChar .* -group "%val{hook_param_capture_1}-indent" javascript-indent-on-char
    hook window InsertChar \n -group "%val{hook_param_capture_1}-insert" javascript-insert-on-new-line
    hook window InsertChar \n -group "%val{hook_param_capture_1}-indent" javascript-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* "
        remove-hooks window %val{hook_param_capture_1}-.+
    "
}

hook -group javascript-highlight global WinSetOption filetype=javascript %{
    add-highlighter window/javascript ref javascript
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/javascript }
}

hook -group typescript-highlight global WinSetOption filetype=typescript %{
    add-highlighter window/typescript ref typescript
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/typescript }
}


provide-module javascript %§

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden javascript-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft x 1s^(\h+)$<ret> d }
}

define-command -hidden javascript-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m s \A|.\z <ret> 1<a-&> /
    >
>

define-command -hidden javascript-insert-on-new-line %<
    evaluate-commands -draft -itersel %<
    execute-keys <semicolon>
    try %[
        evaluate-commands -draft -save-regs '/"' %[
            # copy the commenting prefix
            execute-keys -save-regs '' k x1s^\h*(//+\h*)<ret> y
            try %[
                # if the previous comment isn't empty, create a new one
                execute-keys x<a-K>^\h*//+\h*$<ret> jxs^\h*<ret>P
            ] catch %[
                # if there is no text in the previous comment, remove it completely
                execute-keys d
            ]
        ]
    ]
    try %[
        # if the previous line isn't within a comment scope, break
        execute-keys -draft kx <a-k>^(\h*/\*|\h+\*(?!/))<ret>

        # find comment opening, validate it was not closed, and check its using star prefixes
        execute-keys -draft <a-?>/\*<ret><a-H> <a-K>\*/<ret> <a-k>\A\h*/\*([^\n]*\n\h*\*)*[^\n]*\n\h*.\z<ret>

        try %[
            # if the previous line is opening the comment, insert star preceeded by space
            execute-keys -draft kx<a-k>^\h*/\*<ret>
            execute-keys -draft i*<space><esc>
        ] catch %[
           try %[
                # if the next line is a comment line insert a star
                execute-keys -draft jx<a-k>^\h+\*<ret>
                execute-keys -draft i*<space><esc>
            ] catch %[
                try %[
                    # if the previous line is an empty comment line, close the comment scope
                    execute-keys -draft kx<a-k>^\h+\*\h+$<ret> x1s\*(\h*)<ret>c/<esc>
                ] catch %[
                    # if the previous line is a non-empty comment line, add a star
                    execute-keys -draft i*<space><esc>
                ]
            ]
        ]

        # trim trailing whitespace on the previous line
        try %[ execute-keys -draft s\h+$<ret> d ]
        # align the new star with the previous one
        execute-keys Kx1s^[^*]*(\*)<ret>&
    ]
    >
>

define-command -hidden javascript-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
    execute-keys <semicolon>
    try %<
        # if previous line is part of a comment, do nothing
        execute-keys -draft <a-?>/\*<ret> <a-K>^\h*[^/*\h]<ret>
    > catch %<
        # else if previous line closed a paren (possibly followed by words and a comment),
        # copy indent of the opening paren line
        execute-keys -draft kx 1s(\))(\h+\w+)*\h*(\;\h*)?(?://[^\n]+)?\n\z<ret> m<a-semicolon>J <a-S> 1<a-&>
    > catch %<
        # else indent new lines with the same level as the previous one
        execute-keys -draft K <a-&>
    >
    # remove previous empty lines resulting from the automatic indent
    try %< execute-keys -draft k x <a-k>^\h+$<ret> Hd >
    # indent after an opening brace or parenthesis at end of line
    try %< execute-keys -draft k x <a-k>[{(]\h*$<ret> j <a-gt> >
    # indent after a label (works for case statements)
    try %< execute-keys -draft k x s[a-zA-Z0-9_-]+:\h*$<ret> j <a-gt> >
    # indent after a statement not followed by an opening brace
    try %< execute-keys -draft k x s\)\h*(?://[^\n]+)?\n\z<ret> \
                               <a-semicolon>mB <a-k>\A\b(if|for|while)\b<ret> <a-semicolon>j <a-gt> >
    try %< execute-keys -draft k x s \belse\b\h*(?://[^\n]+)?\n\z<ret> \
                               j <a-gt> >
    # deindent after a single line statement end
    try %< execute-keys -draft K x <a-k>\;\h*(//[^\n]+)?$<ret> \
                               K x s\)(\h+\w+)*\h*(//[^\n]+)?\n([^\n]*\n){2}\z<ret> \
                               MB <a-k>\A\b(if|for|while)\b<ret> <a-S>1<a-&> >
    try %< execute-keys -draft K x <a-k>\;\h*(//[^\n]+)?$<ret> \
                               K x s \belse\b\h*(?://[^\n]+)?\n([^\n]*\n){2}\z<ret> \
                               <a-S>1<a-&> >
    # deindent closing brace(s) when after cursor
    try %< execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <esc> m <a-S> 1<a-&> >
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
    >
>

# Highlighting and hooks bulder for JavaScript and TypeScript
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
define-command -hidden init-javascript-filetype -params 1 %~
    # Highlighters
    # ‾‾‾‾‾‾‾‾‾‾‾‾

    add-highlighter "shared/%arg{1}" regions
    add-highlighter "shared/%arg{1}/code" default-region group
    add-highlighter "shared/%arg{1}/double_string" region '"'  (?<!\\)(\\\\)*"         fill string
    add-highlighter "shared/%arg{1}/single_string" region "'"  (?<!\\)(\\\\)*'         fill string
    add-highlighter "shared/%arg{1}/literal"       region "`"  (?<!\\)(\\\\)*`         group
    add-highlighter "shared/%arg{1}/comment_line"  region //   '$'                     fill comment
    add-highlighter "shared/%arg{1}/comment"       region /\*  \*/                     fill comment
    add-highlighter "shared/%arg{1}/shebang"       region ^#!  $                       fill meta
    add-highlighter "shared/%arg{1}/division" region '[\w\)\]]\K(/|(\h+/\s+))' '(?=\w)' group # Help Kakoune to better detect /…/ literals
    add-highlighter "shared/%arg{1}/regex"         region /    (?<!\\)(\\\\)*/[gimuy]* fill meta
    add-highlighter "shared/%arg{1}/jsx"           region -recurse (?<![\w<])<[a-zA-Z>][\w:.-]* (?<![\w<])<[a-zA-Z>][\w:.-]*(?!\hextends)(?=[\s/>])(?!>\()) (</.*?>|/>) regions

    # Regular expression flags are: g → global match, i → ignore case, m → multi-lines, u → unicode, y → sticky
    # https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp

    add-highlighter "shared/%arg{1}/literal/"       fill string
    add-highlighter "shared/%arg{1}/literal/"       regex \$\{.*?\} 0:value

    add-highlighter "shared/%arg{1}/code/" regex (?:^|[^$_])\b(document|false|null|parent|self|this|true|undefined|window)\b 1:value
    add-highlighter "shared/%arg{1}/code/" regex "-?\b[0-9]*\.?[0-9]+" 0:value
    add-highlighter "shared/%arg{1}/code/" regex \b(Array|Boolean|Date|Function|Number|Object|RegExp|String|Symbol)\b 0:type

    # jsx: In well-formed xml the number of opening and closing tags match up regardless of tag name.
    #
    # We inline a small XML highlighter here since it anyway need to recurse back up to the starting highlighter.
    # To make things simple we assume that jsx is always enabled.

    add-highlighter "shared/%arg{1}/jsx/tag"  region -recurse <  <(?=[/a-zA-Z>]) (?<!=)> regions
    add-highlighter "shared/%arg{1}/jsx/expr" region -recurse \{ \{             \}      ref %arg{1}

    add-highlighter "shared/%arg{1}/jsx/tag/base" default-region group
    add-highlighter "shared/%arg{1}/jsx/tag/double_string" region =\K" (?<!\\)(\\\\)*" fill string
    add-highlighter "shared/%arg{1}/jsx/tag/single_string" region =\K' (?<!\\)(\\\\)*' fill string
    add-highlighter "shared/%arg{1}/jsx/tag/expr" region -recurse \{ \{   \}           group

    add-highlighter "shared/%arg{1}/jsx/tag/base/" regex (\w+) 1:attribute
    add-highlighter "shared/%arg{1}/jsx/tag/base/" regex </?([\w-$]+) 1:keyword
    add-highlighter "shared/%arg{1}/jsx/tag/base/" regex (</?|/?>) 0:meta

    add-highlighter "shared/%arg{1}/jsx/tag/expr/"   ref %arg{1}

    # Keywords are collected at
    # https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#Keywords
    # https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/get
    # https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/set
    add-highlighter "shared/%arg{1}/code/" regex \b(async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|finally|for|function|get|if|import|in|instanceof|let|new|of|return|set|static|super|switch|throw|try|typeof|var|void|while|with|yield)\b 0:keyword
~

init-javascript-filetype javascript
init-javascript-filetype typescript

# Highlighting specific to TypeScript
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
add-highlighter shared/typescript/code/ regex \b(array|boolean|date|number|object|regexp|string|symbol)\b 0:type

# Keywords grabbed from https://github.com/Microsoft/TypeScript/issues/2536
add-highlighter shared/typescript/code/ regex \b(as|constructor|declare|enum|from|implements|interface|module|namespace|package|private|protected|public|readonly|static|type)\b 0:keyword

§

# Aliases
# ‾‾‾‾‾‾‾
provide-module typescript %{ require-module javascript }
