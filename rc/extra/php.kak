# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](php) %{
    set buffer filetype php
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter -group / regions -default code php  \
    double_string '"'  (?<!\\)(\\\\)*" '' \
    single_string "'"  (?<!\\)(\\\\)*' '' \
    comment       //   '$'             '' \
    comment       /\*  \*/             '' \
    comment       '#'  '$'             ''

add-highlighter -group /php/double_string fill string
add-highlighter -group /php/single_string fill string
add-highlighter -group /php/comment       fill comment

add-highlighter -group /php/code regex \$\w* 0:identifier
add-highlighter -group /php/code regex \b(false|null|parent|self|this|true)\b 0:value
add-highlighter -group /php/code regex "-?[0-9]*\.?[0-9]+" 0:value
add-highlighter -group /php/code regex \b((string|int|bool)|[A-Z][a-z].*?)\b 0:type
add-highlighter -group /php/code regex (?<=\W)/[^\n/]+/[gimy]* 0:meta

# Keywords are collected at
# http://php.net/manual/en/reserved.keywords.php
add-highlighter -group /php/code regex \b(__halt_compiler|abstract|and|array|as|break|callable|case|catch|class|clone|const|continue|declare|default|die|do|echo|else|elseif|empty|enddeclare|endfor|endforeach|endif|endswitch|endwhile|eval|exit|extends|final|finally|for|foreach|function|global|goto|if|implements|include|include_once|instanceof|insteadof|interface|isset|list|namespace|new|or|print|private|protected|public|require|require_once|return|static|switch|throw|trait|try|unset|use|var|while|xor|yield|__CLASS__|__DIR__|__FILE__|__FUNCTION__|__LINE__|__METHOD__|__NAMESPACE__|__TRAIT__)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden php-filter-around-selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden php-indent-on-char %<
    eval -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ exec -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \`|.\' <ret> 1<a-&> /
    >
>

def -hidden php-indent-on-new-line %<
    eval -draft -itersel %<
        # copy // comments prefix and following white spaces
        try %{ exec -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        # preserve previous line indent
        try %{ exec -draft \; K <a-&> }
        # filter previous line
        try %{ exec -draft k : php-filter-around-selections <ret> }
        # indent after lines beginning / ending with opener token
        try %_ exec -draft k <a-x> <a-k> ^\h*[[{]|[[{]$ <ret> j <a-gt> _
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group php-highlight global WinSetOption filetype=php %{ add-highlighter ref php }

hook global WinSetOption filetype=php %{
    hook window InsertEnd  .* -group php-hooks  php-filter-around-selections
    hook window InsertChar .* -group php-indent php-indent-on-char
    hook window InsertChar \n -group php-indent php-indent-on-new-line
}

hook -group php-highlight global WinSetOption filetype=(?!php).* %{ remove-highlighter php }

hook global WinSetOption filetype=(?!php).* %{
    remove-hooks window php-indent
    remove-hooks window php-hooks
}
