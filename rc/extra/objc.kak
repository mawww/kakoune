hook global BufCreate .*\.m %{
    set buffer filetype objc
}

def -hidden objc-trim-autoindent %[ eval -draft -itersel %[
    # remove the line if it's empty when leaving the insert mode
    try %[ exec <a-x> 1s^(\h+)$<ret> d ]
] ]

def -hidden objc-indent-on-newline %< eval -draft -itersel %<
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

def -hidden objc-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden objc-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-:><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

def -hidden objc-insert-on-closing-curly-brace %[
    # add a semicolon after a closing brace if part of a class, union or struct definition
    try %[ exec -itersel -draft hm<a-x>B<a-x><a-k>\`\h*(class|struct|union)<ret> a\;<esc> ]
]

def -hidden objc-insert-on-newline %[ eval -draft %[
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

add-highlighter -group / regions -default code -match-capture objc \
    string %{@?(?<!')"} %{(?<!\\)(?:\\\\)*"} "" \
    string %{R"([^(]*)\(} %{\)([^)]*)"} "" \
    comment /\* \*/ "" \
    comment // $ "" \
    disabled ^\h*?#\h*if\h+(?:0|FALSE)\b "#\h*(?:else|elif|endif)" "#\h*if(?:def)?" \
    macro %{^\h*?\K#} %{(?<!\\)\n} ""

add-highlighter -group /objc/string fill string
add-highlighter -group /objc/comment fill comment
add-highlighter -group /objc/disabled fill rgb:666666
add-highlighter -group /objc/macro fill meta
add-highlighter -group /objc/macro regex ^\h*#include\h+(\S*) 1:module
add-highlighter -group /objc/code regex %{\b-?\d+[fdiu]?|'((\\.)?|[^'\\])'} 0:value

%sh{
    # Grammar
    keywords="while|for|if|else|do|switch|case|default|goto|break|continue|return"
    attributes="const|auto|inline|static|volatile|struct|enum|union|typedef"
    attributes="${attributes}|extern|__block|nonatomic|assign|copy|strong"
    attributes="${attributes}|retain|weak|readonly|IBAction|IBOutlet"
    types="void|char|short|int|long|signed|unsigned|float|bool|size_t"
    types="${types}|instancetype|BOOL|NSInteger|NSUInteger|CGFloat|NSString"
    values="self|nil|id|super|TRUE|FALSE|YES|NO|NULL"
    decorators="property|synthesize|interface|implementation|protocol|end"
    decorators="${decorators}|selector|autoreleasepool|try|catch|class|synchronized"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=objc %{
        set window static_words '${keywords}:${attributes}:${types}:${values}:${decorators}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        add-highlighter -group /objc/code regex \b(${keywords})\b 0:keyword
        add-highlighter -group /objc/code regex \b(${attributes})\b 0:attribute
        add-highlighter -group /objc/code regex \b(${types})\b 0:type
        add-highlighter -group /objc/code regex \b(${values})\b 0:value
        add-highlighter -group /objc/code regex @(${decorators})\b 0:attribute
    "
}

hook global WinSetOption filetype=objc %[
    hook -group objc-indent window InsertEnd .* objc-trim-autoindent
    hook -group objc-insert window InsertChar \n objc-insert-on-newline
    hook -group objc-indent window InsertChar \n objc-indent-on-newline
    hook -group objc-indent window InsertChar \{ objc-indent-on-opening-curly-brace
    hook -group objc-indent window InsertChar \} objc-indent-on-closing-curly-brace
    hook -group objc-insert window InsertChar \} objc-insert-on-closing-curly-brace

    alias window alt objc-alternative-file
]

hook global WinSetOption filetype=(?!objc$).* %[
    remove-hooks window objc-hooks
    remove-hooks window objc-indent
    remove-hooks window objc-insert

    unalias window alt objc-alternative-file
]

hook -group objc-highlight global WinSetOption filetype=objc %[ add-highlighter ref objc ]
hook -group objc-highlight global WinSetOption filetype=(?!objc$).* %[ remove-highlighter objc ]

decl -docstring "colon separated list of path in which header files will be looked for" \
    str-list alt_dirs ".:.."

def objc-alternative-file -docstring "Jump to the alternate file (header/implementation)" %{ %sh{
    alt_dirs=$(printf %s\\n "${kak_opt_alt_dirs}" | tr ':' '\n')
    file="${kak_buffile##*/}"
    file_noext="${file%.*}"
    dir=$(dirname "${kak_buffile}")

    case ${file} in
        *.m)
            for alt_dir in ${alt_dirs}; do
                altname="${dir}/${alt_dir}/${file_noext}.h"
                if [ -f "${altname}" ]; then
                    printf 'edit %%{%s}\n' "${altname}"
                    exit
                fi
            done
        ;;
        *.h)
            for alt_dir in ${alt_dirs}; do
                altname="${dir}/${alt_dir}/${file_noext}.m"
                if [ -f "${altname}" ]; then
                    printf 'edit %%{%s}\n' "${altname}"
                    exit
                fi
            done
        ;;
        *)
            echo "echo -color Error 'extension not recognized'"
            exit
        ;;
    esac
    echo "echo -color Error 'alternative file not found'"
}}
