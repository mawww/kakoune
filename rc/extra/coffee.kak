# http://coffeescript.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](coffee) %{
    set-option buffer filetype coffee
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/ regions -default code coffee     \
    double_string '"""' '"""'                '' \
    single_string "'''" "'''"                '' \
    comment       '###' '###'                '' \
    regex         '///' ///[gimy]*           '' \
    double_string '"' (?<!\\)(\\\\)*"        '' \
    single_string "'" "'"                    '' \
    regex         '/' (?<!\\)(\\\\)*/[gimy]* '' \
    comment       '#' '$'                    ''

# Regular expression flags are: g → global match, i → ignore case, m → multi-lines, y → sticky
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp

add-highlighter shared/coffee/double_string fill string
add-highlighter shared/coffee/double_string regions regions interpolation \Q#{ \} \{
add-highlighter shared/coffee/double_string/regions/interpolation fill meta
add-highlighter shared/coffee/single_string fill string
add-highlighter shared/coffee/regex fill meta
add-highlighter shared/coffee/regex regions regions interpolation \Q#{ \} \{
add-highlighter shared/coffee/regex/regions/interpolation fill meta
add-highlighter shared/coffee/comment fill comment

# Keywords are collected at
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#Keywords
# http://coffeescript.org/documentation/docs/lexer.html#section-63
add-highlighter shared/coffee/code regex [$@]\w* 0:variable
add-highlighter shared/coffee/code regex \b(Array|Boolean|Date|Function|Number|Object|RegExp|String)\b 0:type
add-highlighter shared/coffee/code regex \b(document|false|no|null|off|on|parent|self|this|true|undefined|window|yes)\b 0:value
add-highlighter shared/coffee/code regex \b(and|is|isnt|not|or)\b 0:operator
add-highlighter shared/coffee/code regex \b(break|case|catch|class|const|continue|debugger|default|delete|do|else|enum|export|extends|finally|for|function|if|implements|import|in|instanceof|interface|let|native|new|package|private|protected|public|return|static|super|switch|throw|try|typeof|var|void|while|with|yield)\b 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden coffee-filter-around-selections %{
    evaluate-commands -draft -itersel %{
        execute-keys <a-x>
        # remove trailing white spaces
        try %{ execute-keys -draft s \h + $ <ret> d }
    }
}

define-command -hidden coffee-indent-on-new-line %{
    evaluate-commands -draft -itersel %{
        # copy '#' comment prefix and following white spaces
        try %{ execute-keys -draft k <a-x> s '^\h*\K#\h*' <ret> y gh j P }
        # preserve previous line indent
        try %{ execute-keys -draft \; K <a-&> }
        # filter previous line
        try %{ execute-keys -draft k : coffee-filter-around-selections <ret> }
        # indent after start structure
        try %{ execute-keys -draft k <a-x> <a-k> ^ \h * (case|catch|class|else|finally|for|function|if|switch|try|while|with) \b | (=|->) $ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook -group coffee-highlight global WinSetOption filetype=coffee %{ add-highlighter window ref coffee }

hook global WinSetOption filetype=coffee %{
    hook window InsertEnd  .* -group coffee-hooks  coffee-filter-around-selections
    hook window InsertChar \n -group coffee-indent coffee-indent-on-new-line
}

hook -group coffee-highlight global WinSetOption filetype=(?!coffee).* %{ remove-highlighter window/coffee }

hook global WinSetOption filetype=(?!coffee).* %{
    remove-hooks window coffee-indent
    remove-hooks window coffee-hooks
}
