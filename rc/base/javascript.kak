# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](js) %{
    set buffer filetype javascript
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code javascript \
    double_string '"'  (?<!\\)(\\\\)*"        '' \
    single_string "'"  (?<!\\)(\\\\)*'        '' \
    literal       "`"  (?<!\\)(\\\\)*`        '' \
    comment       //   '$'                    '' \
    comment       /\*  \*/                    ''

# Regular expression flags are: g → global match, i → ignore case, m → multi-lines, y → sticky
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp

addhl -group /javascript/double_string fill string
addhl -group /javascript/single_string fill string
addhl -group /javascript/comment       fill comment
addhl -group /javascript/literal       fill string
addhl -group /javascript/literal       regex \${.*?} 0:value

addhl -group /javascript/code regex \$\w* 0:identifier
addhl -group /javascript/code regex \b(document|false|null|parent|self|this|true|undefined|window)\b 0:value
addhl -group /javascript/code regex "-?[0-9]*\.?[0-9]+" 0:value
addhl -group /javascript/code regex \b(Array|Boolean|Date|Function|Number|Object|RegExp|String)\b 0:type
addhl -group /javascript/code regex (?<=\W)/[^\n/]+/[gimy]* 0:meta

# Keywords are collected at
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#Keywords
addhl -group /javascript/code regex \b(break|case|catch|class|const|continue|debugger|default|delete|do|else|export|extends|finally|for|function|if|import|in|instanceof|let|new|of|return|super|switch|throw|try|typeof|var|void|while|with|yield)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _javascript_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _javascript_indent_on_char %<
    eval -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ exec -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \`|.\' <ret> 1<a-&> /
    >
>

def -hidden _javascript_indent_on_new_line %<
    eval -draft -itersel %<
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _javascript_filter_around_selections <ret> }
        # copy // comments prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y gh j P }
        # indent after lines beginning / ending with opener token
        try %_ exec -draft k x <a-k> ^\h*[[{]|[[{]$ <ret> j <a-gt> _
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group javascript-highlight global WinSetOption filetype=javascript %{ addhl ref javascript }

hook global WinSetOption filetype=javascript %{
    hook window InsertEnd  .* -group javascript-hooks  _javascript_filter_around_selections
    hook window InsertChar .* -group javascript-indent _javascript_indent_on_char
    hook window InsertChar \n -group javascript-indent _javascript_indent_on_new_line
}

hook -group javascript-highlight global WinSetOption filetype=(?!javascript).* %{ rmhl javascript }

hook global WinSetOption filetype=(?!javascript).* %{
    rmhooks window javascript-indent
    rmhooks window javascript-hooks
}
