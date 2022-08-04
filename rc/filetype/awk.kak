# Detection
# ---------

hook global BufCreate .*\.awk %{
    set-option buffer filetype awk
}

# Initialization
# --------------

hook global WinSetOption filetype=awk %{
    require-module awk
    
    hook window InsertChar \n -group awk-indent awk-indent-on-new-line
    hook window ModeChange pop:insert:.* -group awk-trim-indent awk-trim-indent

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window awk-.+ }
}

hook -group awk-highlight global WinSetOption filetype=awk %{
    add-highlighter window/awk ref awk
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/awk }
}

provide-module awk %@

# Highlighters
# ------------

add-highlighter shared/awk regions
add-highlighter shared/awk/code default-region group
add-highlighter shared/awk/comment region '#' '$' fill comment
add-highlighter shared/awk/string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/awk/regex region (\A|[^\s\w)\]+-]|\bcase)\s*\K/ (?<!\\)(\\\\)*/ fill attribute

add-highlighter shared/awk/code/ regex (\.\d+|\b\d+\.?\d*)([eE][+-]?\d+)?\b 0:value # Decimal/octal/scientific
add-highlighter shared/awk/code/ regex \b0[xX][\da-fA-F]+\b 0:value # Hexadecimal
add-highlighter shared/awk/code/ regex \$|\+|-|\*|/|%|\^|=|&&|\||!|<|>|\?|~ 0:operator

evaluate-commands %sh{
    # Grammar
    patterns="BEGIN END BEGINFILE ENDFILE"
    variables="BINMODE CONVFMT FIELDWIDTHS FPAT FS IGNORECASE LINT OFMT OFS
        ORS PREC ROUNDMODE RS SUBSEP TEXTDOMAIN ARGC ARGV ARGIND ENVIRON
        ERRNO FILENAME FNR NF FUNCTAB NR PROCINFO RLENGTH RSTART RT SYMTAB"
    keywords="break continue delete exit function getline next print printf
        return switch nextfile func if else while for do"
    functions="atan2 cos exp int intdiv log rand sin sqrt srand asort asort1
        gensub gsub index length match patsplit split sprintf strtonum sub
        substr tolower toupper close fflush system mktime strftime systime
        and compl lshift or rshift xor isarray typeof bindtextdomain dcgettext
        dcngetext"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=awk %{
        set-option window static_words $(join "${patterns} ${variables} ${keywords} ${functions}" ' ')
    }"

    # Highlight keywords
    printf %s\\n "add-highlighter shared/awk/code/ regex \b($(join "${patterns}" '|'))\b 0:type"
    printf %s\\n "add-highlighter shared/awk/code/ regex \b($(join "${variables}" '|'))\b 0:meta"
    printf %s\\n "add-highlighter shared/awk/code/ regex \b($(join "${keywords}" '|'))\b 0:keyword"
    printf %s\\n "add-highlighter shared/awk/code/ regex \b($(join "${functions}" '|'))\b 0:function"
}

# Commands
# --------

define-command -hidden awk-indent-on-new-line %[
    evaluate-commands -draft -itersel %[
        # preserve previous line indent
        try %[ execute-keys -draft <semicolon> K <a-&> ]
        # cleanup trailing whitespaces from previous line
        try %[ execute-keys -draft k x s \h+$ <ret> d ]
        # indent after line ending in opening curly brace
        try %[ execute-keys -draft kx <a-k>\{\h*(#.*)?$<ret> j<a-gt> ]
        # deindent closing brace when after cursor
        try %[ execute-keys -draft x <a-k> ^\h*\} <ret> gh / \} <ret> m <a-S> 1<a-&> ]
    ]
]

define-command -hidden awk-trim-indent %{
    try %{ execute-keys -draft <semicolon> x s ^\h+$ <ret> d }
}

@
