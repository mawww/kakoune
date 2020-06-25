# http://mlton.org/MLBasis
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.mlb %{
    set-option buffer filetype mlb
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=mlb %{
    require-module mlb
    set-option buffer extra_word_chars '_' '-' '.'
    set-option window static_words %opt{mlb_static_words}
}

hook -group mlb-highlight global WinSetOption filetype=mlb %{
    add-highlighter window/mlb ref mlb
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/mlb }
}

provide-module mlb %[

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/mlb regions
add-highlighter shared/mlb/code default-region group
add-highlighter shared/mlb/string region '"' '(?<!\\)(\\\\)*"' group
add-highlighter shared/mlb/string/fill fill string
add-highlighter shared/mlb/comment region -recurse '\(\*' '\(\*' '\*\)' fill comment

evaluate-commands %sh{
    keywords='basis bas and open local let in end structure signature functor ann'

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    printf %s\\n "declare-option str-list mlb_static_words $(join "${keywords}" ' ')"
    printf %s\\n "add-highlighter shared/mlb/code/ regex (?<![\w'-/.])($(join "${keywords}" '|'))(?![\w'-/.]) 0:keyword"
}
add-highlighter shared/mlb/code/ regex "=" 0:operator
add-highlighter shared/mlb/code/ regex "\b([A-Z][\w']*)\b" 0:type
add-highlighter shared/mlb/code/ regex "\b[A-Z]{2}[A-Z0-9_']+\b" 0:attribute
add-highlighter shared/mlb/code/ regex "\$\(\w+\)" 0:variable
add-highlighter shared/mlb/string/ regex "\$\(\w*\)" 0:variable

]
