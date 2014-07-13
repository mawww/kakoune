hook global BufCreate .*\.(c|cc|cpp|cxx|C|h|hh|hpp|hxx|H) %{
    set buffer filetype cpp
}

hook global BufSetOption mimetype=text/x-c(\+\+)? %{
    set buffer filetype cpp
}

def -hidden _cpp_indent_on_new_line %~
    eval -draft -itersel %_
        # preserve previous line indent
        try %{ exec -draft <space>K<a-&> }
        # indent after lines ending with { or (
        try %[ exec -draft k<a-x> <a-k> [{(]\h*$ <ret> j<a-gt> ]
        # cleanup trailing white space son previous line
        try %{ exec -draft k<a-x> s \h+$ <ret>d }
        # align to opening paren of previous line
        try %{ exec -draft [( <a-k> \`\([^\n]+\n[^\n]*\n?\' <ret> s \`\(\h*.|.\' <ret> & }
        # align to previous statement start when previous line closed a parenthesis
        # try %{ exec -draft <a-?>\)M<a-k>\`\(.*\)[^\n()]*\n\h*\n?\'<ret>s\`|.\'<ret>1<a-&> }
        # copy // comments prefix
        try %{ exec -draft k<a-x> s ^\h*\K(/{2,}) <ret> yjglp }
        # indent after visibility specifier
        try %[ exec -draft k<a-x> <a-k> ^\h*(public|private|protected):\h*$ <ret> j<a-gt> ]
        # indent after if|else|while|for
        try %[ exec -draft <space><a-F>)MB <a-k> \`(if|else|while|for)\h*\(.*\)\h*\n\h*\n?\' <ret> s \`|.\' <ret> 1<a-&>1<a-space><a-gt> ]
    _
~

def -hidden _cpp_indent_on_opening_curly_brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden _cpp_indent_on_closing_curly_brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
    # add ; after } if class or struct definition
    try %[ exec -draft "hm<space><a-?>(class|struct)<ret><a-k>\`(class|struct)[^{}\n]+(\n)?\s*\{\'<ret><a-space>ma;<esc>" ]
]

addhl -group / multi_region -default code cpp \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ '' \
    macro ^\h*?\K# (?<!\\)\n ''

addhl -group /cpp/string fill string
addhl -group /cpp/comment fill comment
addhl -group /cpp/macro fill meta

addhl -group /cpp/code regex "\<(this|true|false|NULL|nullptr|)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" 0:value
addhl -group /cpp/code regex "\<(void|int|char|unsigned|float|bool|size_t)\>" 0:type
addhl -group /cpp/code regex "\<(while|for|if|else|do|switch|case|default|goto|break|continue|return|using|try|catch|throw|new|delete|and|or|not|operator|explicit)\>" 0:keyword
addhl -group /cpp/code regex "\<(const|mutable|auto|namespace|inline|static|volatile|class|struct|enum|union|public|protected|private|template|typedef|virtual|friend|extern|typename|override|final)\>" 0:attribute

hook global WinSetOption filetype=cpp %[
    addhl ref cpp

    # cleanup trailing whitespaces when exiting insert mode
    hook window InsertEnd .* -group cpp-hooks %{ try %{ exec -draft <a-x>s\h+$<ret>d } }

    hook window InsertChar \n -group cpp-indent _cpp_indent_on_new_line
    hook window InsertChar \{ -group cpp-indent _cpp_indent_on_opening_curly_brace
    hook window InsertChar \} -group cpp-indent _cpp_indent_on_closing_curly_brace
]

hook global WinSetOption filetype=(?!cpp).* %{
    rmhl cpp
    rmhooks window cpp-indent
    rmhooks window cpp-hooks
}

def -hidden _cpp_insert_include_guards %{
    exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>ggxyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>
}

hook global BufNew .*\.(h|hh|hpp|hxx|H) _cpp_insert_include_guards

decl str-list alt_dirs ".;.."

def alt -docstring "Jump to the alternate file (header/implementation)" %{ %sh{
    alt_dirs=$(echo ${kak_opt_alt_dirs} | sed -e 's/;/ /g')
    file=$(basename ${kak_buffile})
    dir=$(dirname ${kak_buffile})

    case ${file} in
         *.c|*.cc|*.cpp|*.cxx|*.C)
             for alt_dir in ${alt_dirs}; do
                 for ext in h hh hpp hxx H; do
                     altname="${dir}/${alt_dir}/${file%.*}.${ext}"
                     [ -f ${altname} ] && break
                 done
                 [ -f ${altname} ] && break
             done
         ;;
         *.h|*.hh|*.hpp|*.hxx|*.H)
             for alt_dir in ${alt_dirs}; do
                 for ext in c cc cpp cxx C; do
                     altname="${dir}/${alt_dir}/${file%.*}.${ext}"
                     [ -f ${altname} ] && break
                 done
                 [ -f ${altname} ] && break
             done
         ;;
    esac
    if [ -f ${altname} ]; then
       echo edit "'${altname}'"
    else
       echo echo "'alternative file not found'"
    fi
}}
