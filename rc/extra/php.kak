# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](php) %{
    set buffer filetype php
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code php  \
    double_string '"'  (?<!\\)(\\\\)*" '' \
    single_string "'"  (?<!\\)(\\\\)*' '' \
    comment       //   '$'             '' \
    comment       /\*  \*/             ''

addhl -group /php/double_string fill string
addhl -group /php/single_string fill string
addhl -group /php/comment       fill comment

addhl -group /php/code regex \$\w* 0:identifier
addhl -group /php/code regex \b(false|null|parent|self|this|true)\b 0:value
addhl -group /php/code regex "-?[0-9]*\.?[0-9]+" 0:value
addhl -group /php/code regex \b((string|int|bool)|[A-Z][a-z].*?)\b 0:type
addhl -group /php/code regex (?<=\W)/[^\n/]+/[gimy]* 0:meta

# Keywords are collected at
# http://php.net/manual/en/reserved.keywords.php
addhl -group /php/code regex \b(__halt_compiler|abstract|and|array|as|break|callable|case|catch|class|clone|const|continue|declare|default|die|do|echo|else|elseif|empty|enddeclare|endfor|endforeach|endif|endswitch|endwhile|eval|exit|extends|final|finally|for|foreach|function|global|goto|if|implements|include|include_once|instanceof|insteadof|interface|isset|list|namespace|new|or|print|private|protected|public|require|require_once|return|static|switch|throw|trait|try|unset|use|var|while|xor|yield|__CLASS__|__DIR__|__FILE__|__FUNCTION__|__LINE__|__METHOD__|__NAMESPACE__|__TRAIT__)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _php_filter_around_selections %{
    # remove trailing white spaces
    try %{ exec -draft -itersel <a-x> s \h+$ <ret> d }
}

def -hidden _php_indent_on_char %<
    eval -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ exec -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \`|.\' <ret> 1<a-&> /
    >
>

def -hidden _php_indent_on_new_line %<
    eval -draft -itersel %<
        # preserve previous line indent
        try %{ exec -draft <space> K <a-&> }
        # filter previous line
        try %{ exec -draft k : _php_filter_around_selections <ret> }
        # copy // comments prefix and following white spaces
        try %{ exec -draft k x s ^\h*\K#\h* <ret> y j p }
        # indent after lines beginning / ending with opener token
        try %_ exec -draft k x <a-k> ^\h*[[{]|[[{]$ <ret> j <a-gt> _
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group php-highlight global WinSetOption filetype=php %{ addhl ref php }

hook global WinSetOption filetype=php %{
    hook window InsertEnd  .* -group php-hooks  _php_filter_around_selections
    hook window InsertChar .* -group php-indent _php_indent_on_char
    hook window InsertChar \n -group php-indent _php_indent_on_new_line
}

hook -group php-highlight global WinSetOption filetype=(?!php).* %{ rmhl php }

hook global WinSetOption filetype=(?!php).* %{
    rmhooks window php-indent
    rmhooks window php-hooks
}
