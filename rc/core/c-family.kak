hook global BufCreate .*\.(cc|cpp|cxx|C|hh|hpp|hxx|H)$ %{
    set buffer filetype cpp
    set buffer mimetype ''
}

hook global BufCreate .*\.c$ %{
    set buffer filetype c
    set buffer mimetype ''
}

hook global BufCreate .*\.h$ %{
    try %{
        exec -draft %{%s\b::\b|\btemplate\h*<lt>|\bclass\h+\w+|\b(typename|namespace)\b|\b(public|private|protected)\h*:<ret>}
        set buffer filetype cpp
    } catch %{
        set buffer filetype c
    }
    set buffer mimetype ''
}

hook global BufSetOption mimetype=text/x-c %{
    set buffer filetype c
}

hook global BufSetOption mimetype=text/x-c\+\+ %{
    set buffer filetype cpp
}

hook global BufCreate .*\.m %{
    set buffer filetype objc
    set buffer mimetype ''
}

hook global BufSetOption mimetype=text/x-objc %{
    set buffer filetype objc
}

def -hidden _c-family-trim-autoindent %[ eval -draft -itersel %[
    ## remove the line if it's empty when leaving the insert mode
    try %[ exec <a-x> 1s^(\h+)$<ret> d ]
] ]

def -hidden _c-family-indent-on-newline %[ eval -draft -itersel %[
    exec \;
    try %[
        ## if previous line closed a paren, copy indent of the opening paren line
        exec -draft k<a-x> 1s(\))(\h+\w+)*\h*(\;\h*)?$<ret> m<a-\;>J s\`|.\'<ret> 1<a-&>
    ] catch %[
        ## else indent new lines with the same level as the previous one
        exec -draft K <a-&>
    ]
    ## remove previous empty lines resulting from the automatic indent
    try %[ exec -draft k <a-x>H <a-k>^\h+$<ret> d ]
    ## indent after an opening brace
    try %[ exec -draft K s\{\h*$<ret> j <a-gt> ]
    ## indent after a label
    try %[ exec -draft k <a-x> s[a-zA-Z0-9_-]+:\h*$<ret> j <a-gt> ]
    ## indent after a statement not followed by an opening brace
    try %[ exec -draft k <a-x> <a-k>\b(if|else|for|while)\h*\(.+?\)\h*$<ret> j <a-gt> ]
    ## align to the opening parenthesis on a previous line if its followed by text on the same line
    try %[ exec -draft {b <a-k>\`\([^\n]+\n[^\n]*\'<ret> L s\`|.\'<ret> & ]
] ]

def -hidden _c-family-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ exec -draft -itersel h<a-F>)M <a-k> \`\(.*\)\h*\n\h*\{\' <ret> s \`|.\' <ret> 1<a-&> ]
]

def -hidden _c-family-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ exec -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\`|.\'<ret>1<a-&> ]
]

def -hidden _c-family-insert-on-closing-curly-brace %[
    # add a semicolon after a closing brace if part of a class, union or struct definition
    try %[ exec -itersel -draft hm<a-x>B<a-x><a-k>^\h*(class|struct|union)<ret> a\;<esc> ]
]

def -hidden _c-family-insert-on-newline %[ eval -draft %[
    exec \;
    try %[
        eval -draft %[
            ## copy the commenting prefix
            exec -save-regs '' k <a-x>1s^\h*(//+\h*)<ret> y
            try %[
                ## if the previous comment isn't empty, create a new one
                exec <a-x><a-K>^\h*//+\h*$<ret> j<a-x>s^\h*<ret>p
            ] catch %[
                ## if there is no text in the previous comment, remove it completely
                exec d
            ]
        ]
    ]
    try %[
        ## select the previous line
        exec k <a-x>
        ## if the previous line isn't within a comment scope, break
        exec <a-k>^(\h*/\*|\h+\*[^/])<ret>
        ## simple test to check that the previous comment has been left open
        exec <a-K>\*/\h*$<ret>

        try %[
            ## if the next line is a comment line, add a star
            exec -draft 2j<a-x><a-k>^\h+\*<ret>
            exec -draft j<a-x>s^\h*<ret>a*<space><esc>
        ] catch %[
            try %[
                ## if the previous line is an empty comment line, close the comment scope
                exec -draft <a-k>^\h+\*\h+$<ret> <a-x>1s\*(\h*)<ret>c/<esc>
            ] catch %[
                ## if the previous line is a non-empty comment line, add a star
                exec -draft j<a-x>s^\h*<ret>a*<space><esc>
            ]
        ]

        ## trim trailing whitespace on the previous line
        try %[ exec -draft 1s(\h+)$<ret>d ]
        ## align the new star with the previous one
        exec J<a-x>1s^[^*]*(\*)<ret>&
    ]
] ]

# Regions definition are the same between c++ and objective-c
%sh{
    for ft in c cpp objc; do
        if [ "${ft}" = "objc" ]; then
            maybe_at='@?'
        else
            maybe_at=''
        fi

        printf %s\\n '
            addhl -group / regions -default code FT \
                string %{MAYBEAT(?<!QUOTE)"} %{(?<!\\)(\\\\)*"} "" \
                comment /\* \*/ "" \
                comment // $ "" \
                disabled ^\h*?#\h*if\h+(0|FALSE)\b "#\h*(else|elif|endif)" "#\h*if(def)?" \
                macro %{^\h*?\K#} %{(?<!\\)\n} ""

            addhl -group /FT/string fill string
            addhl -group /FT/comment fill comment
            addhl -group /FT/disabled fill rgb:666666
            addhl -group /FT/macro fill meta' | sed -e "s/FT/${ft}/g; s/QUOTE/'/g; s/MAYBEAT/${maybe_at}/;"
    done
}

# c specific
addhl -group /c/code regex %{\bNULL\b|\b-?(0x[0-9a-fA-F]+|\d+)[fdiu]?|'((\\.)?|[^'\\])'} 0:value
%sh{
    # Grammar
    keywords="while|for|if|else|do|switch|case|default|goto|asm|break|continue|return|sizeof"
    attributes="const|auto|register|inline|static|volatile|struct|enum|union|typedef|extern|restrict"
    types="void|char|short|int|long|signed|unsigned|float|double|size_t"

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=c %{
        set window static_words '${keywords}:${attributes}:${types}'
    }" | sed 's,|,:,g'

    # Highlight keywords
    printf %s "
        addhl -group /c/code regex \b(${keywords})\b 0:keyword
        addhl -group /c/code regex \b(${attributes})\b 0:attribute
        addhl -group /c/code regex \b(${types})\b 0:type
    "
}

# c++ specific
addhl -group /cpp/code regex %{\b-?(0x[0-9a-fA-F]+|\d+)[fdiu]?|'((\\.)?|[^'\\])'} 0:value

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
        addhl -group /cpp/code regex \b(${keywords})\b 0:keyword
        addhl -group /cpp/code regex \b(${attributes})\b 0:attribute
        addhl -group /cpp/code regex \b(${types})\b 0:type
        addhl -group /cpp/code regex \b(${values})\b 0:value
    "
}

# objective-c specific
addhl -group /objc/code regex %{\b-?\d+[fdiu]?|'((\\.)?|[^'\\])'} 0:value

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
        addhl -group /objc/code regex \b(${keywords})\b 0:keyword
        addhl -group /objc/code regex \b(${attributes})\b 0:attribute
        addhl -group /objc/code regex \b(${types})\b 0:type
        addhl -group /objc/code regex \b(${values})\b 0:value
        addhl -group /objc/code regex @(${decorators})\b 0:attribute
    "
}

hook global WinSetOption filetype=(c|cpp|objc) %[
    try %{ # we might be switching from one c-family language to another
        rmhooks window c-family-hooks
        rmhooks window c-family-indent
    }

    hook -group c-family-indent window InsertEnd .* _c-family-trim-autoindent
    hook -group c-family-indent window InsertChar \n _c-family-indent-on-newline
    hook -group c-family-indent window InsertChar \{ _c-family-indent-on-opening-curly-brace
    hook -group c-family-indent window InsertChar \} _c-family-indent-on-closing-curly-brace
    hook -group c-family-insert window InsertChar \} _c-family-insert-on-closing-curly-brace
    hook -group c-family-insert window InsertChar \n _c-family-insert-on-newline

    alias window alt c-family-alternative-file
]

hook global WinSetOption filetype=(?!(c|cpp|objc)$).* %[
    rmhooks window c-family-hooks
    rmhooks window c-family-indent
    rmhooks window c-family-insert

    unalias window alt c-family-alternative-file
]

hook -group c-highlight global WinSetOption filetype=c %[ addhl ref c ]
hook -group c-highlight global WinSetOption filetype=(?!c$).* %[ rmhl c ]

hook -group cpp-highlight global WinSetOption filetype=cpp %[ addhl ref cpp ]
hook -group cpp-highlight global WinSetOption filetype=(?!cpp$).* %[ rmhl cpp ]

hook -group objc-highlight global WinSetOption filetype=objc %[ addhl ref objc ]
hook -group objc-highlight global WinSetOption filetype=(?!objc$).* %[ rmhl objc ]

decl str c_include_guard_style "ifdef"
def -hidden _c-family-insert-include-guards %{
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

hook global BufNew .*\.(h|hh|hpp|hxx|H) _c-family-insert-include-guards

decl str-list alt_dirs ".;.."

def c-family-alternative-file -docstring "Jump to the alternate file (header/implementation)" %{ %sh{
    alt_dirs=$(printf %s\\n "${kak_opt_alt_dirs}" | sed -e 's/;/ /g')
    file=$(basename "${kak_buffile}")
    dir=$(dirname "${kak_buffile}")

    case ${file} in
        *.c|*.cc|*.cpp|*.cxx|*.C|*.inl|*.m)
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
                for ext in c cc cpp cxx C m; do
                    altname="${dir}/${alt_dir}/${file%.*}.${ext}"
                    [ -f ${altname} ] && break
                done
                [ -f ${altname} ] && break
            done
        ;;
        *)
            echo "echo -color Error 'extension not recognized'"
            exit
        ;;
    esac
    if [ -f ${altname} ]; then
       printf %s\\n "edit '${altname}'"
    else
       echo "echo -color Error 'alternative file not found'"
    fi
}}
