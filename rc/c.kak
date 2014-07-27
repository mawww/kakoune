hook global BufCreate .*\.(c|h) %{
    set buffer filetype c
}

hook global BufSetOption mimetype=text/x-c %{
    set buffer filetype c
}

def -hidden _c_indent_on_new_line %~
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

def -hidden _c_indent_on_opening_curly_brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden _c_indent_on_closing_curly_brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
    # add ; after } if struct or union definition
    try %[ exec -draft "hm<space><a-?>(struct|union)<ret><a-k>\`(struct|union)[^{}\n]+(\n)?\s*\{\'<ret><a-space>ma;<esc>" ]
]

addhl -group / regions -default code c \
    string %{(?<!')"} %{(?<!\\)(\\\\)*"} '' \
    comment /\* \*/ '' \
    comment // $ '' \
    macro ^\h*?\K# (?<!\\)\n ''

addhl -group /c/string fill string
addhl -group /c/comment fill comment
addhl -group /c/macro fill meta

addhl -group /c/code regex "\<(__FILE__|__FUNCTION__|__LINE__|__func__|NULL|true|false|)\>|\<-?\d+[fdiu]?|'((\\.)?|[^'\\])'" 0:value
addhl -group /c/code regex "\<(_Bool|bool|char|double|float|int|short|s?size_t|struct|union|unsigned|void|w(char|int)_t|u?int(_fast|_least)?(8|16|32|64)_t|u?int(max|ptr)_t)\>" 0:type
addhl -group /c/code regex "\<(break|case|continue|default|do|else|for|goto|if|return|switch|while)\>" 0:keyword
addhl -group /c/code regex "\<(__attribute__|__cdecl|__forceinline|__inline|__inline__|__restrict|__restrict__|_Atomic|_Noreturn|const|enum|extern|inline|register|restrict|static|volatile|typedef)\>" 0:attribute

hook global WinSetOption filetype=c %[
    addhl ref c

    # cleanup trailing whitespaces when exiting insert mode
# Broken:
#    hook window InsertEnd .* -group c-hooks %{ try %{ exec -draft <a-x>s\h+$<ret>d } }

    hook window InsertChar \n -group c-indent _c_indent_on_new_line
    hook window InsertChar \{ -group c-indent _c_indent_on_opening_curly_brace
    hook window InsertChar \} -group c-indent _c_indent_on_closing_curly_brace
]

hook global WinSetOption filetype=(?!c).* %{
    rmhl c
    rmhooks window c-indent
    rmhooks window c-hooks
}

def -hidden _c_insert_include_guards %{
    exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>ggxyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>
}

# This doesn't appear to be working:
# hook global BufNew .*\.(h) _c_insert_include_guards

decl str-list alt_dirs ".;.."

def alt -docstring "Jump to the alternate file (header/implementation)" %{ %sh{
    alt_dirs=$(echo ${kak_opt_alt_dirs} | sed -e 's/;/ /g')
    file=$(basename ${kak_buffile})
    dir=$(dirname ${kak_buffile})

    case ${file} in
         *.c)
             for alt_dir in ${alt_dirs}; do
                 altname="${dir}/${alt_dir}/${file%.*}.h"
                 [ -f ${altname} ] && break
             done
         ;;
         *.h)
             for alt_dir in ${alt_dirs}; do
                     altname="${dir}/${alt_dir}/${file%.*}.c"
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
