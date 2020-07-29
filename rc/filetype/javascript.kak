# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.]m?(js)x? %{
    set-option buffer filetype javascript
}

hook global BufCreate .*[.](ts)x? %{
    set-option buffer filetype typescript
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=(javascript|typescript) %{
    require-module javascript

    hook window ModeChange pop:insert:.* -group "%val{hook_param_capture_1}-trim-indent" javascript-trim-indent
    hook window InsertChar .* -group "%val{hook_param_capture_1}-indent" javascript-indent-on-char
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
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : javascript-trim-indent <ret> }
        # indent after lines beginning / ending with opener token
        try %_ execute-keys -draft k <a-x> s [[({] <ret> <space> <a-l> <a-K> [\])}] <ret> j <a-gt> _
        # deindent closing token(s) when after cursor
        try %_ execute-keys -draft <a-x> <a-k> ^\h*[})\]] <ret> gh / [})\]] <ret> m <a-S> 1<a-&> _
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
    add-highlighter "shared/%arg{1}/jsx"           region -recurse (?<![\w<])<[a-zA-Z>][\w:.-]* (?<![\w<])<[a-zA-Z>][\w:.-]*(?!\hextends)(?=[\s/>])(?!>\()) (</.*?>|/>) regions
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
