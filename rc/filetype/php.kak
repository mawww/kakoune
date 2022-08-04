# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](phpt?) %{
    set-option buffer filetype php
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=php %{
    require-module php

    hook window ModeChange pop:insert:.* -group php-trim-indent php-trim-indent
    hook window InsertChar .* -group php-indent php-indent-on-char
    hook window InsertChar \n -group php-insert php-insert-on-new-line
    hook window InsertChar \n -group php-indent php-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window php-.+ }
}

hook -group php-highlight global WinSetOption filetype=php %{
    add-highlighter window/php-file ref php-file
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/php-file }
}

provide-module php %§
require-module html

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/php regions
add-highlighter shared/php/code  default-region group
add-highlighter shared/php/double_string region '"'    (?<!\\)(\\\\)*" group
add-highlighter shared/php/single_string region "'"    (?<!\\)(\\\\)*' fill string
add-highlighter shared/php/doc_comment   region ///    '$'             group
add-highlighter shared/php/doc_comment2  region /\*\*  \*/             ref php/doc_comment
add-highlighter shared/php/comment1      region //     '$'             fill comment
add-highlighter shared/php/comment2      region /\*    \*/             fill comment
add-highlighter shared/php/comment3      region '#'    '$'             fill comment
add-highlighter shared/php/heredoc       region -match-capture '<<<(.*?)$' '^\h*(.*?);' fill string


add-highlighter shared/php/code/ regex &?\$\w* 0:variable
add-highlighter shared/php/code/ regex \b(false|null|parent|self|this|true)\b 0:value
add-highlighter shared/php/code/ regex "(\b|-)[0-9]*\.?[0-9]+\b" 0:value
add-highlighter shared/php/code/ regex \b((string|int|bool)|[A-Z][a-z].*?)\b 0:type
add-highlighter shared/php/code/ regex \B/[^\n/]+/[gimy]* 0:meta
add-highlighter shared/php/code/ regex '<\?(php)?|\?>' 0:meta

add-highlighter shared/php/double_string/ fill string
add-highlighter shared/php/double_string/ regex (?<!\\)(\\\\)*(\$\w+)(->\w+)* 0:variable
add-highlighter shared/php/double_string/ regex \{(?<!\\)(\\\\)*(\$\w+)(->\w+)*\} 0:variable

# Highlight doc comments
add-highlighter shared/php/doc_comment/ fill string
add-highlighter shared/php/doc_comment/ regex '`.*`' 0:module
add-highlighter shared/php/doc_comment/ regex '@\w+' 0:meta

# Keywords are collected at
# http://php.net/manual/en/reserved.keywords.php
add-highlighter shared/php/code/ regex \b(__halt_compiler|abstract|and|array|as|break|callable|case|catch|class|clone|const|continue|declare|default|die|do|echo|else|elseif|empty|enddeclare|endfor|endforeach|endif|endswitch|endwhile|eval|exit|extends|final|finally|for|foreach|function|global|goto|if|implements|include|include_once|instanceof|insteadof|interface|isset|list|namespace|new|or|print|private|protected|public|require|require_once|return|static|switch|throw|trait|try|unset|use|var|while|xor|yield|__CLASS__|__DIR__|__FILE__|__FUNCTION__|__LINE__|__METHOD__|__NAMESPACE__|__TRAIT__)\b 0:keyword

# Highlighter for html with php tags in it, i.e. the structure of conventional php files
add-highlighter shared/php-file regions
add-highlighter shared/php-file/html default-region ref html
add-highlighter shared/php-file/php  region '<\?(php)?'     '\?>'      ref php

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden php-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden php-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %/ execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m s \A|.\z <ret> 1<a-&> /
    >
>

define-command -hidden php-insert-on-new-line %<
    evaluate-commands -draft -itersel %<
        # copy // comments or docblock * prefix and following white spaces
        try %{ execute-keys -draft s [^/] <ret> k x s ^\h*\K(?://|[*][^/])\h* <ret> y gh j P }
        # append " * " on lines starting a multiline /** or /* comment
        try %{ execute-keys -draft k x s ^\h*/[*][* ]? <ret> j gi i <space>*<space> }
    >
>

define-command -hidden php-indent-on-new-line %<
    evaluate-commands -draft -itersel %<
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon> K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : php-trim-indent <ret> }
        # indent after lines beginning / ending with opener token
        try %_ execute-keys -draft k x <a-k> ^\h*[[{]|[[{]$ <ret> j <a-gt> _
        # deindent closer token(s) when after cursor
        try %_ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> _
    >
>

§
