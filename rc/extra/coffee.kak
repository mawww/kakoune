# http://coffeescript.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# require commenting.kak

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufSetOption mimetype=text/x-coffee %{
    set buffer filetype coffee
}

hook global BufCreate .*[.](coffee) %{
    set buffer filetype coffee
}

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

addhl -group / regions -default code coffee     \
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

addhl -group /coffee/double_string fill string
addhl -group /coffee/double_string regions regions interpolation \Q#{ \} \{
addhl -group /coffee/double_string/regions/interpolation fill meta
addhl -group /coffee/single_string fill string
addhl -group /coffee/regex fill meta
addhl -group /coffee/regex regions regions interpolation \Q#{ \} \{
addhl -group /coffee/regex/regions/interpolation fill meta
addhl -group /coffee/comment fill comment

# Keywords are collected at
# https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#Keywords
# http://coffeescript.org/documentation/docs/lexer.html#section-63
addhl -group /coffee/code regex [$@]\w* 0:identifier
addhl -group /coffee/code regex \<(Array|Boolean|Date|Function|Number|Object|RegExp|String)\> 0:type
addhl -group /coffee/code regex \<(document|false|no|null|off|on|parent|self|this|true|undefined|window|yes)\> 0:value
addhl -group /coffee/code regex \<(and|is|isnt|not|or)\> 0:operator
addhl -group /coffee/code regex \<(break|case|catch|class|const|continue|debugger|default|delete|do|else|enum|export|extends|finally|for|function|if|implements|import|in|instanceof|interface|let|native|new|package|private|protected|public|return|static|super|switch|throw|try|typeof|var|void|while|with|yield)\> 0:keyword

# Commands
# ‾‾‾‾‾‾‾‾

def -hidden _coffee_filter_around_selections %{
    eval -draft -itersel %{
        exec <a-x>
        # remove trailing white spaces
        try %{ exec -draft s \h + $ <ret> d }
    }
}

def -hidden _coffee_indent_on_new_line %{
    eval -draft -itersel %{
        # preserve previous line indent
        try %{ exec -draft K <a-&> }
        # filter previous line
        try %{ exec -draft k : _coffee_filter_around_selections <ret> }
        # copy '#' comment prefix and following white spaces
        try %{ exec -draft k x s ^ \h * \K \# \h * <ret> y j p }
        # indent after start structure
        try %{ exec -draft k x <a-k> ^ \h * (case|catch|class|else|finally|for|function|if|switch|try|while|with) \b | (=|->) $ <ret> j <a-gt> }
    }
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=coffee %{
    addhl ref coffee

    hook window InsertEnd  .* -group coffee-hooks  _coffee_filter_around_selections
    hook window InsertChar \n -group coffee-indent _coffee_indent_on_new_line

    set window comment_line_chars '#'
    set window comment_selection_chars '###:###'
}

hook global WinSetOption filetype=(?!coffee).* %{
    rmhl coffee
    rmhooks window coffee-indent
    rmhooks window coffee-hooks
}
