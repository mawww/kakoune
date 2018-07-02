# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](js)x? %{
    set-option buffer filetype javascript
}

hook global BufCreate .*[.](ts)x? %{
    set-option buffer filetype typescript
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden javascript-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden javascript-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m s \A|.\z <ret> 1<a-&> /
    >
>

define-command -hidden javascript-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : javascript-filter-around-selections <ret> }
        # indent after lines beginning / ending with opener token
        try %_ execute-keys -draft k <a-x> <a-k> ^\h*[[{]|[[{]$ <ret> j <a-gt> _
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
    add-highlighter "shared/%arg{1}/regex"         region /    (?<!\\)(\\\\)*/[gimuy]* fill meta
    add-highlighter "shared/%arg{1}/jsx"           region -recurse (?<![\w<])<[a-zA-Z][\w:.-]* (?<![\w<])<[a-zA-Z][\w:.-]*(?!\hextends)(?=[\s/>])(?!>\()) (</.*?>|/>) regions
    add-highlighter "shared/%arg{1}/division" region '[\w\)\]]\K(/|(\h+/\h+))' '(?=\w)' group # Help Kakoune to better detect /…/ literals

    # Regular expression flags are: g → global match, i → ignore case, m → multi-lines, u → unicode, y → sticky
    # https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp

    add-highlighter "shared/%arg{1}/literal/"       fill string
    add-highlighter "shared/%arg{1}/literal/"       regex \$\{.*?\} 0:value

    add-highlighter "shared/%arg{1}/code/" regex [^$_]\b(document|false|null|parent|self|this|true|undefined|window)\b 1:value
    add-highlighter "shared/%arg{1}/code/" regex "-?\b[0-9]*\.?[0-9]+" 0:value
    add-highlighter "shared/%arg{1}/code/" regex \b(Array|Boolean|Date|Function|Number|Object|RegExp|String|Symbol)\b 0:type

    # jsx: In well-formed xml the number of opening and closing tags match up regardless of tag name.
    #
    # We inline a small XML highlighter here since it anyway need to recurse back up to the starting highlighter.
    # To make things simple we assume that jsx is always enabled.

    add-highlighter "shared/%arg{1}/jsx/tag"  region -recurse <  <(?=[/a-zA-Z]) (?<!=)> regions
    add-highlighter "shared/%arg{1}/jsx/expr" region -recurse \{ \{             \}      ref %arg{1}
        

    add-highlighter "shared/%arg{1}/jsx/tag/base" default-region group
    add-highlighter "shared/%arg{1}/jsx/tag/double_string" region =\K" (?<!\\)(\\\\)*" fill string
    add-highlighter "shared/%arg{1}/jsx/tag/single_string" region =\K' (?<!\\)(\\\\)*' fill string
    add-highlighter "shared/%arg{1}/jsx/tag/expr" region -recurse \{ \{   \}           group

    add-highlighter "shared/%arg{1}/jsx/tag/base/" regex (\w+) 1:attribute
    add-highlighter "shared/%arg{1}/jsx/tag/base/" regex </?([\w-$]+) 1:keyword
    add-highlighter "shared/%arg{1}/jsx/tag/base/" regex (</?|/?>) 0:meta

    add-highlighter "shared/%arg{1}/jsx/tag/expr/"   fill default,default+e
    add-highlighter "shared/%arg{1}/jsx/tag/expr/"   ref %arg{1}

    # Keywords are collected at
    # https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#Keywords
    add-highlighter "shared/%arg{1}/code/" regex \b(async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|finally|for|function|if|import|in|instanceof|let|new|of|return|static|super|switch|throw|try|typeof|var|void|while|with|yield)\b 0:keyword

    # Initialization
    # ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

    hook -group "%arg{1}-highlight" global WinSetOption "filetype=%arg{1}" "add-highlighter window/%arg{1} ref %arg{1}"

    hook global WinSetOption "filetype=%arg{1}" "
        hook window ModeChange insert:.* -group %arg{1}-hooks  javascript-filter-around-selections
        hook window InsertChar .* -group %arg{1}-indent javascript-indent-on-char
        hook window InsertChar \n -group %arg{1}-indent javascript-indent-on-new-line
    "

    hook -group "%arg{1}-highlight" global WinSetOption "filetype=(?!%arg{1}).*" "remove-highlighter window/%arg{1}"

    hook global WinSetOption "filetype=(?!%arg{1}).*" "
        remove-hooks window %arg{1}-indent
        remove-hooks window %arg{1}-hooks
    "
~

init-javascript-filetype javascript
init-javascript-filetype typescript

# Highlighting specific to TypeScript
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
add-highlighter shared/typescript/code/ regex \b(array|boolean|date|number|object|regexp|string|symbol)\b 0:type

# Keywords grabbed from https://github.com/Microsoft/TypeScript/issues/2536
add-highlighter shared/typescript/code/ regex \b(as|constructor|declare|enum|from|implements|interface|module|namespace|package|private|protected|public|readonly|static|type)\b 0:keyword
