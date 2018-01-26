# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](php) %{
    set-option buffer filetype php
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code php  \
    double_string '"'  (?<!\\)(\\\\)*" '' \
    single_string "'"  (?<!\\)(\\\\)*' '' \
    comment       //   '$'             '' \
    comment       /\*  \*/             '' \
    comment       '#'  '$'             ''

add-highlighter shared/php/double_string fill string
add-highlighter shared/php/single_string fill string
add-highlighter shared/php/comment       fill comment

add-highlighter shared/php/code regex \$\w* 0:variable
add-highlighter shared/php/code regex \b(false|null|parent|self|this|true)\b 0:value
add-highlighter shared/php/code regex "-?[0-9]*\.?[0-9]+" 0:value
add-highlighter shared/php/code regex \b((string|int|bool)|[A-Z][a-z].*?)\b 0:type
add-highlighter shared/php/code regex \B/[^\n/]+/[gimy]* 0:meta

# Keywords are collected at
# http://php.net/manual/en/reserved.keywords.php
add-highlighter shared/php/code regex \b(__halt_compiler|abstract|and|array|as|break|callable|case|catch|class|clone|const|continue|declare|default|die|do|echo|else|elseif|empty|enddeclare|endfor|endforeach|endif|endswitch|endwhile|eval|exit|extends|final|finally|for|foreach|function|global|goto|if|implements|include|include_once|instanceof|insteadof|interface|isset|list|namespace|new|or|print|private|protected|public|require|require_once|return|static|switch|throw|trait|try|unset|use|var|while|xor|yield|__CLASS__|__DIR__|__FILE__|__FUNCTION__|__LINE__|__METHOD__|__NAMESPACE__|__TRAIT__)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden php-filter-around-selections %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel <a-x> s \h+$ <ret> d }
}

define-command -hidden php-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ execute-keys -draft <a-h> <a-k> ^\h+[]}]$ <ret> m s \A|.\z <ret> 1<a-&> /
    >
>

define-command -hidden php-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s ^\h*\K#\h* <ret> y gh j P }
        preserve-previous-line-indent
        # filter previous line
        try %{ execute-keys -draft k : php-filter-around-selections <ret> }
        # indent after lines beginning / ending with opener token
        try %_ execute-keys -draft k <a-x> <a-k> ^\h*[[{]|[[{]$ <ret> j <a-gt> _
    >
>

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group php-highlight global WinSetOption filetype=php %{ add-highlighter window ref php }

hook global WinSetOption filetype=php %{
    hook window ModeChange insert:.* -group php-hooks  php-filter-around-selections
    hook window InsertChar .* -group php-indent php-indent-on-char
    hook window InsertChar \n -group php-indent php-indent-on-new-line
}

hook -group php-highlight global WinSetOption filetype=(?!php).* %{ remove-highlighter window/php }

hook global WinSetOption filetype=(?!php).* %{
    remove-hooks window php-indent
    remove-hooks window php-hooks
}
