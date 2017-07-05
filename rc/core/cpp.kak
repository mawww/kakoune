hook global BufCreate .*\.(cc|cpp|cxx|hh|hpp|hxx)$ %{
    set buffer filetype cpp
}

hook global BufCreate .*\.(h|inl)$ %{
    try %{
        exec -draft %{%s\b::\b|\btemplate\h*<lt>|\bclass\h+\w+|\b(typename|namespace)\b|\b(public|private|protected)\h*:<ret>}
        set buffer filetype cpp
    }
}

def -hidden cpp-trim-autoindent %[ eval -draft -itersel %[
    # remove the line if it's empty when leaving the insert mode
    try %[ exec <a-x> 1s^(\h+)$<ret> d ]
] ]

def -hidden cpp-indent-on-newline %< eval -draft -itersel %<
    exec \;
    try %<
        # if previous line closed a paren, copy indent of the opening paren line
        exec -draft k<a-x> 1s(\))(\h+\w+)*\h*(\;\h*)?$<ret> m<a-\;>J s\`|.\'<ret> 1<a-&>
    > catch %<
        # else indent new lines with the same level as the previous one
        exec -draft K <a-&>
    >
    # remove previous empty lines resulting from the automatic indent
    try %< exec -draft k <a-x> <a-k>^\h+$<ret> Hd >
    # indent after an opening brace
    try %< exec -draft k <a-x> s\{\h*$<ret> j <a-gt> >
    # indent after a label
    try %< exec -draft k <a-x> s[a-zA-Z0-9_-]+:\h*$<ret> j <a-gt> >
    # indent after a statement not followed by an opening brace
    try %< exec -draft k <a-x> <a-k>\b(if|else|for|while)\h*\(.+?\)\h*$<ret> j <a-gt> >
    # align to the opening parenthesis or opening brace (whichever is first)
    # on a previous line if its followed by text on the same line
    try %< eval -draft %<
        # Go to opening parenthesis and opening brace, then select the most nested one
        try %< try %< exec [bZ<a-\;>[B<a-z><gt> > catch %< exec [B > >
        # Validate selection and get first and last char
        exec <a-k>\`[{(](\h*\S+)+\n<ret> <a-:><a-\;>L s\`|.\'<ret>
        # Remove eventual indent from new line
        try %< exec -draft <space> <a-h> s\h+<ret> d >
        # Now align that new line with the opening parenthesis/brace
        exec &
     > >
> >

def -hidden cpp-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden cpp-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-:><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

def -hidden cpp-insert-on-closing-curly-brace %[
    # add a semicolon after a closing brace if part of a class, union or struct definition
    try %[ exec -itersel -draft hm<a-x>B<a-x><a-k>\`\h*(class|struct|union)<ret> a\;<esc> ]
]

def -hidden cpp-insert-on-newline %[ eval -draft %[
    exec \;
    try %[
        eval -draft %[
            # copy the commenting prefix
            exec -save-regs '' k <a-x>1s^\h*(//+\h*)<ret> y
            try %[
                # if the previous comment isn't empty, create a new one
                exec <a-x><a-K>^\h*//+\h*$<ret> j<a-x>s^\h*<ret>P
            ] catch %[
                # if there is no text in the previous comment, remove it completely
                exec d
            ]
        ]
    ]
    try %[
        # if the previous line isn't within a comment scope, break
        exec -draft k<a-x> <a-k>^(\h*/\*|\h+\*(?!/))<ret>

        # find comment opening, validate it was not closed, and check its using star prefixes
        exec -draft <a-?>/\*<ret><a-H> <a-K>\*/<ret> <a-k>\`\h*/\*([^\n]*\n\h*\*)*[^\n]*\n\h*.\'<ret>

        try %[
            # if the previous line is opening the comment, insert star preceeded by space
            exec -draft k<a-x><a-k>^\h*/\*<ret>
            exec -draft i<space>*<space><esc>
        ] catch %[
           try %[
                # if the next line is a comment line insert a star
                exec -draft j<a-x><a-k>^\h+\*<ret>
                exec -draft i*<space><esc>
            ] catch %[
                try %[
                    # if the previous line is an empty comment line, close the comment scope
                    exec -draft k<a-x><a-k>^\h+\*\h+$<ret> <a-x>1s\*(\h*)<ret>c/<esc>
                ] catch %[
                    # if the previous line is a non-empty comment line, add a star
                    exec -draft i*<space><esc>
                ]
            ]
        ]

        # trim trailing whitespace on the previous line
        try %[ exec -draft 1s(\h+)$<ret>d ]
        # align the new star with the previous one
        exec J<a-x>1s^[^*]*(\*)<ret>&
    ]
] ]

add-highlighter -group / regions -default code -match-capture cpp \
    string %{(?<!')"} %{(?<!\\)(?:\\\\)*"} "" \
    string %{R"([^(]*)\(} %{\)([^)]*)"} "" \
    comment /\* \*/ "" \
    comment // $ "" \
    disabled ^\h*?#\h*if\h+(?:0|FALSE)\b "#\h*(?:else|elif|endif)" "#\h*if(?:def)?" \
    macro %{^\h*?\K#} %{(?<!\\)\n} ""

add-highlighter -group /cpp/string fill string
add-highlighter -group /cpp/comment fill comment
add-highlighter -group /cpp/disabled fill rgb:666666
add-highlighter -group /cpp/macro fill meta
add-highlighter -group /cpp/macro regex ^\h*#include\h+(\S*) 1:module
add-highlighter -group /cpp/code regex %{\b-?(0x[0-9a-fA-F]+|\d+)[fdiu]?|'((\\.)?|[^'\\])'} 0:value

%sh{
    # Grammar
    keywords="while|for|if|else|do|switch|case|default|goto|asm|break|continue"
    keywords="${keywords}|return|using|try|catch|throw|new|delete|and|and_eq|or"
    keywords="${keywords}|or_eq|not|operator|explicit|reinterpret_cast"
    keywords="${keywords}|const_cast|static_cast|dynamic_cast|sizeof|alignof"
    keywords="${keywords}|alignas|decltype"
    attributes="const|constexpr|mutable|auto|noexcept|namespace|inline|static"
    attributes="${attributes}|volatile|class|struct|enum|union|public|protected"
    attributes="${attributes}|private|template|typedef|virtual|friend|extern"
    attributes="${attributes}|typename|override|final"
    types="void|char|short|int|long|signed|unsigned|float|double|size_t|bool"
    values="this|true|false|NULL|nullptr"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=cpp %{
        set window static_words '${keywords}:${attributes}:${types}:${values}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        add-highlighter -group /cpp/code regex \b(${keywords})\b 0:keyword
        add-highlighter -group /cpp/code regex \b(${attributes})\b 0:attribute
        add-highlighter -group /cpp/code regex \b(${types})\b 0:type
        add-highlighter -group /cpp/code regex \b(${values})\b 0:value
    "
}

# c and c++ compiler macros
add-highlighter -group /cpp/code regex \b(__cplusplus|__STDC_HOSTED__|__FILE__|__LINE__|__DATE__|__TIME__|__STDCPP_DEFAULT_NEW_ALIGNMENT__)\b 0:builtin

hook global WinSetOption filetype=cpp %[
    hook -group cpp-indent window InsertEnd .* cpp-trim-autoindent
    hook -group cpp-insert window InsertChar \n cpp-insert-on-newline
    hook -group cpp-indent window InsertChar \n cpp-indent-on-newline
    hook -group cpp-indent window InsertChar \{ cpp-indent-on-opening-curly-brace
    hook -group cpp-indent window InsertChar \} cpp-indent-on-closing-curly-brace
    hook -group cpp-insert window InsertChar \} cpp-insert-on-closing-curly-brace

    alias window alt cpp-alternative-file
]

hook global WinSetOption filetype=(?!cpp$).* %[
    remove-hooks window cpp-hooks
    remove-hooks window cpp-indent
    remove-hooks window cpp-insert

    unalias window alt cpp-alternative-file
]

hook -group cpp-highlight global WinSetOption filetype=cpp %[ add-highlighter ref cpp ]
hook -group cpp-highlight global WinSetOption filetype=(?!cpp$).* %[ remove-highlighter cpp ]

decl -docstring %{control the type of include guard to be inserted in empty headers
Can be one of the following:
 ifdef: old style ifndef/define guard
 pragma: newer type of guard using "pragma once"} \
    str c_include_guard_style "ifdef"

def -hidden cpp-insert-include-guards %{
    %sh{
        case "${kak_opt_c_include_guard_style}" in
            ifdef)
                echo 'exec ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>ggxyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>'
                ;;
            pragma)
                echo 'exec ggi#pragma<space>once<esc>'
                ;;
            *);;
        esac
    }
}

hook -group c-family-insert global BufNewFile .*\.(h|hh|hpp|hxx) cpp-insert-include-guards

decl -docstring "colon separated list of path in which header files will be looked for" \
    str-list alt_dirs ".:.."

def cpp-alternative-file -docstring "Jump to the alternate file (header/implementation)" %{ %sh{
    alt_dirs=$(printf %s\\n "${kak_opt_alt_dirs}" | tr ':' '\n')
    file="${kak_buffile##*/}"
    file_noext="${file%.*}"
    dir=$(dirname "${kak_buffile}")

    case ${file} in
        *.cc|*.cpp|*.cxx)
            for alt_dir in ${alt_dirs}; do
                for ext in h hh hpp hxx inl; do
                    altname="${dir}/${alt_dir}/${file_noext}.${ext}"
                    if [ -f "${altname}" ]; then
                        printf 'edit %%{%s}\n' "${altname}"
                        exit
                    fi
                done
            done
        ;;
        *.h|*.hh|*.hpp|*.hxx|*.inl)
            for alt_dir in ${alt_dirs}; do
                for ext in cc cpp cxx; do
                    altname="${dir}/${alt_dir}/${file_noext}.${ext}"
                    if [ -f "${altname}" ]; then
                        printf 'edit %%{%s}\n' "${altname}"
                        exit
                    fi
                done
            done
        ;;
        *)
            echo "echo -color Error 'extension not recognized'"
            exit
        ;;
    esac
    echo "echo -color Error 'alternative file not found'"
}}
