# https://smlfamily.github.io
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.(sml|fun|sig) %{
    set-option buffer filetype sml
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=sml %{
    require-module sml
    set-option buffer extra_word_chars '_' "'"
    set-option window static_words %opt{sml_static_words}
}

hook -group sml-highlight global WinSetOption filetype=sml %{
    add-highlighter window/sml ref sml
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/sml }
}

provide-module sml %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/sml regions
add-highlighter shared/sml/code default-region group
add-highlighter shared/sml/string region '#?"' '(?<!\\)(\\\\)*"' fill string
add-highlighter shared/sml/comment region -recurse '\(\*' '\(\*' '\*\)' fill comment

evaluate-commands %sh{
    keywords='abstype and andalso as case datatype do else end exception fn fun
              handle if in infix infixr let local nonfix of op open orelse raise
              rec then type val with withtype while eqtype functor include
              sharing sig signature struct structure'
    types='unit exn ref'
    ops='before ignore o
         div mod quot rem abs
         not chr ord ceil floor round trunc
         andb orb xorb notb'

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    printf %s\\n "declare-option str-list sml_static_words $(join "${keywords} ${types} ${ops}" ' ')"

    printf %s "
        add-highlighter shared/sml/code/ regex \b($(join "${keywords}" '|'))\b 0:keyword
        add-highlighter shared/sml/code/ regex \b($(join "${types}" '|'))\b 0:builtin
        add-highlighter shared/sml/code/ regex \b($(join "${ops}" '|'))\b 0:operator
    "
}

# Symbolic identifiers
add-highlighter shared/sml/code/ regex "[!*/+\-~\^@=<>%%&$?`\\#:|]+" 0:operator

# Record projection functions
add-highlighter shared/sml/code/ regex "(?<![!*/+\-~\^@=<>%%&$?`\\#:|])#([\w']+)?(?![!*/+\-~\^@=<>%%&$?`\\#:|])" 0:function

# Symbolic keywords
add-highlighter shared/sml/code/ regex "(?<![!*/+\-~\^@=<>%%&$?`\\#:|])(=>|=|\*|->|:>|:|;|\.\.\.|\b_\b|\|)(?![!*/+\-~\^@=<>%%&$?`\\#:|])" 0:keyword

# Type variables
add-highlighter shared/sml/code/ regex "(?<![\w'])'[\w']+(?![\w'])" 0:variable

# Structure identifiers and value constructors
add-highlighter shared/sml/code/ regex "(?<![\w'])([A-Z][\w']*\.?)" 0:type

# Signature identifiers and all-caps value constructors
add-highlighter shared/sml/code/ regex "(?<![\w'])[A-Z]{2}[A-Z0-9_']+(?![\w'])" 0:attribute

# Constants
add-highlighter shared/sml/code/ regex "(?<![\w'])(true|false|nil)\b" 0:value

# Numeric literals
add-highlighter shared/sml/code/ regex "(?<![\w'])0w[0-9]+\b" 0:value
add-highlighter shared/sml/code/ regex "(?<![\w'])(0wx|0xw)[0-9a-fA-F]+\b" 0:value
add-highlighter shared/sml/code/ regex "(?<![\w'])(0wb|0bw)[01]+\b" 0:value
add-highlighter shared/sml/code/ regex "(~|(?<![\w']))0x[0-9a-fA-F]+\b" 0:value
add-highlighter shared/sml/code/ regex "(~|(?<![\w']))0b[01]+\b" 0:value
add-highlighter shared/sml/code/ regex "(?<!#)(~|(?<![\w']))[0-9]+(\.[0-9]+)?([eE]~?[0-9]+)?\b" 0:value

]
