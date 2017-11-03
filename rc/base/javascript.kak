# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](js) %{
    set-option buffer filetype javascript
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code javascript \
    double_string '"'  (?<!\\)(\\\\)*"         '' \
    single_string "'"  (?<!\\)(\\\\)*'         '' \
    literal       "`"  (?<!\\)(\\\\)*`         '' \
    comment       //   '$'                     '' \
    comment       /\*  \*/                     '' \
    regex         /    (?<!\\)(\\\\)*/[gimuy]* '' \
    division '[\w\)\]](/|(\h+/\h+))' '\w' '' # Help Kakoune to better detect /…/ literals

# Regular expression flags are: g → global match, i → ignore case, m → multi-lines, u → unicode, y → sticky
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp

add-highlighter shared/javascript/double_string fill string
add-highlighter shared/javascript/single_string fill string
add-highlighter shared/javascript/regex         fill meta
add-highlighter shared/javascript/comment       fill comment
add-highlighter shared/javascript/literal       fill string
add-highlighter shared/javascript/literal       regex \$\{.*?\} 0:value

add-highlighter shared/javascript/code regex \$\w* 0:variable
add-highlighter shared/javascript/code regex \b(document|false|null|parent|self|this|true|undefined|window)\b 0:value
add-highlighter shared/javascript/code regex "-?[0-9]*\.?[0-9]+" 0:value
add-highlighter shared/javascript/code regex \b(Array|Boolean|Date|Function|Number|Object|RegExp|String|Symbol)\b 0:type

# Keywords are collected at
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#Keywords
add-highlighter shared/javascript/code regex \b(async|await|break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|finally|for|function|if|import|in|instanceof|let|new|of|return|super|switch|throw|try|typeof|var|void|while|with|yield)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden javascript-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden javascript-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ execute-keys -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \A|.\z <ret> 1<a-&> /
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

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group javascript-highlight global WinSetOption filetype=javascript %{ add-highlighter window ref javascript }

hook global WinSetOption filetype=javascript %{
    hook window InsertEnd  .* -group javascript-hooks  javascript-filter-around-selections
    hook window InsertChar .* -group javascript-indent javascript-indent-on-char
    hook window InsertChar \n -group javascript-indent javascript-indent-on-new-line
}

hook -group javascript-highlight global WinSetOption filetype=(?!javascript).* %{ remove-highlighter window/javascript }

hook global WinSetOption filetype=(?!javascript).* %{
    remove-hooks window javascript-indent
    remove-hooks window javascript-hooks
}
