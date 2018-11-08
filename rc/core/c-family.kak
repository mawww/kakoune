hook global BufCreate .*\.(cc|cpp|cxx|C|hh|hpp|hxx|H)$ %{
    set-option buffer filetype cpp
}

hook global BufSetOption filetype=c\+\+ %{
    set-option buffer filetype cpp
}

hook global BufCreate .*\.c$ %{
    set-option buffer filetype c
}

hook global BufCreate .*\.h$ %{
    try %{
        execute-keys -draft %{%s\b::\b|\btemplate\h*<lt>|\bclass\h+\w+|\b(typename|namespace)\b|\b(public|private|protected)\h*:<ret>}
        set-option buffer filetype cpp
    } catch %{
        set-option buffer filetype c
    }
}

hook global BufCreate .*\.m %{
    set-option buffer filetype objc
}

define-command -hidden c-family-trim-autoindent %{
    # remove the line if it's empty when leaving the insert mode
    try %{ execute-keys -draft <a-x> 1s^(\h+)$<ret> d }
}

define-command -hidden c-family-indent-on-newline %< evaluate-commands -draft -itersel %<
    execute-keys \;
    try %<
        # if previous line is part of a comment, do nothing
        execute-keys -draft <a-?>/\*<ret> <a-K>^\h*[^/*\h]<ret>
    > catch %<
        # else if previous line closed a paren, copy indent of the opening paren line
        execute-keys -draft k<a-x> 1s(\))(\h+\w+)*\h*(\;\h*)?$<ret> m<a-\;>J <a-S> 1<a-&>
    > catch %<
        # else indent new lines with the same level as the previous one
        execute-keys -draft K <a-&>
    >
    # remove previous empty lines resulting from the automatic indent
    try %< execute-keys -draft k <a-x> <a-k>^\h+$<ret> Hd >
    # indent after an opening brace or parenthesis at end of line
    try %< execute-keys -draft k <a-x> s[{(]\h*$<ret> j <a-gt> >
    # indent after a label
    try %< execute-keys -draft k <a-x> s[a-zA-Z0-9_-]+:\h*$<ret> j <a-gt> >
    # indent after a statement not followed by an opening brace
    try %< execute-keys -draft k <a-x> <a-k>\b(if|else|for|while)\h*(\(.*?\)\h*)?$<ret> j <a-gt> >
    # deindent after a single line statement end
    try %< execute-keys -draft K <a-x> <a-k>\;\h*$<ret> K <a-x> s\b(if|else|for|while)\h*(\(.*?\)\h*)?$|.\z<ret> 1<a-&> >
    # align to the opening parenthesis or opening brace (whichever is first)
    # on a previous line if its followed by text on the same line
    try %< evaluate-commands -draft %<
        # Go to opening parenthesis and opening brace, then select the most nested one
        try %< execute-keys [c [({],[)}] <ret> >
        # Validate selection and get first and last char
        execute-keys <a-k>\A[{(](\h*\S+)+\n<ret> <a-:><a-\;>L <a-S>
        # Remove eventual indent from new line
        try %< execute-keys -draft <space> <a-h> s\h+<ret> d >
        # Now align that new line with the opening parenthesis/brace
        execute-keys &
     > >
> >

define-command -hidden c-family-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> <a-S> 1<a-&> ]
]

define-command -hidden c-family-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-:><a-k>^\h+\}$<ret>hm<a-S>1<a-&> ]
]

define-command -hidden c-family-insert-on-closing-curly-brace %[
    # add a semicolon after a closing brace if part of a class, union or struct definition
    try %[ execute-keys -itersel -draft hm<a-x>B<a-x><a-k>\A\h*(class|struct|union|enum)<ret> '<a-;>;i;<esc>' ]
]

define-command -hidden c-family-insert-on-newline %[ evaluate-commands -itersel -draft %[
    execute-keys \;
    try %[
        evaluate-commands -draft -save-regs '/"' %[
            # copy the commenting prefix
            execute-keys -save-regs '' k <a-x>1s^\h*(//+\h*)<ret> y
            try %[
                # if the previous comment isn't empty, create a new one
                execute-keys <a-x><a-K>^\h*//+\h*$<ret> j<a-x>s^\h*<ret>P
            ] catch %[
                # if there is no text in the previous comment, remove it completely
                execute-keys d
            ]
        ]
    ]
    try %[
        # if the previous line isn't within a comment scope, break
        execute-keys -draft k<a-x> <a-k>^(\h*/\*|\h+\*(?!/))<ret>

        # find comment opening, validate it was not closed, and check its using star prefixes
        execute-keys -draft <a-?>/\*<ret><a-H> <a-K>\*/<ret> <a-k>\A\h*/\*([^\n]*\n\h*\*)*[^\n]*\n\h*.\z<ret>

        try %[
            # if the previous line is opening the comment, insert star preceeded by space
            execute-keys -draft k<a-x><a-k>^\h*/\*<ret>
            execute-keys -draft i*<space><esc>
        ] catch %[
           try %[
                # if the next line is a comment line insert a star
                execute-keys -draft j<a-x><a-k>^\h+\*<ret>
                execute-keys -draft i*<space><esc>
            ] catch %[
                try %[
                    # if the previous line is an empty comment line, close the comment scope
                    execute-keys -draft k<a-x><a-k>^\h+\*\h+$<ret> <a-x>1s\*(\h*)<ret>c/<esc>
                ] catch %[
                    # if the previous line is a non-empty comment line, add a star
                    execute-keys -draft i*<space><esc>
                ]
            ]
        ]

        # trim trailing whitespace on the previous line
        try %[ execute-keys -draft s\h+$<ret> d ]
        # align the new star with the previous one
        execute-keys K<a-x>1s^[^*]*(\*)<ret>&
    ]
] ]

# Regions definition are the same between c++ and objective-c
evaluate-commands %sh{
    for ft in c cpp objc; do
        if [ "${ft}" = "objc" ]; then
            maybe_at='@?'
        else
            maybe_at=''
        fi

        printf %s\\n '
            add-highlighter shared/FT regions
            add-highlighter shared/FT/code default-region group
            add-highlighter shared/FT/string region %{MAYBEAT(?<!QUOTE)(?<!QUOTE\\)"} %{(?<!\\)(?:\\\\)*"} fill string
            add-highlighter shared/FT/raw_string region %{R"([^(]*)\(} %{\)([^")]*)"} fill string
            add-highlighter shared/FT/comment region /\* \*/ fill comment
            add-highlighter shared/FT/line_comment region // (?<!\\)(?=\n) fill comment
            add-highlighter shared/FT/disabled region -recurse "#\h*if(?:def)?" ^\h*?#\h*if\h+(?:0|FALSE)\b "#\h*(?:else|elif|endif)" fill rgb:666666
            add-highlighter shared/FT/macro region %{^\h*?\K#} %{(?<!\\)(?=\n)|(?=//)} group

            add-highlighter shared/FT/macro/ fill meta
            add-highlighter shared/FT/macro/ regex ^\h*#include\h+(\S*) 1:module
            add-highlighter shared/FT/macro/ regex /\*.*?\*/ 0:comment
            ' | sed -e "s/FT/${ft}/g; s/QUOTE/'/g; s/MAYBEAT/${maybe_at}/;"
    done
}

# c specific
add-highlighter shared/c/code/numbers regex %{\b-?(0x[0-9a-fA-F]+|\d+)([uU][lL]{0,2}|[lL]{1,2}[uU]?|[fFdDiI])?|'((\\.)?|[^'\\])'} 0:value
evaluate-commands %sh{
    # Grammar
    keywords="asm break case continue default do else for goto if return
              sizeof switch while offsetof alignas alignof"
    attributes="auto const enum extern inline register restrict static struct
                typedef union volatile thread_local"
    types="char double float int long short signed unsigned void"
    complex_types="complex imaginary"
    fenv_types=$(echo f{env,except}_t)
    inttypes_types="imaxdiv_t"
    locale_types="lconv"
    math_types=$(echo {float,double}_t)
    setjmp_types="jmp_buf"
    signal_types="sig_atomic_t"
    stdarg_types="va_list"
    stdatomic_types=$(echo memory_order atomic_{bool,{,s,u,w}char,{,u}short,{,u}int,{,u}{,l}long,char{16,32}_t,{,u}int{ptr,max,{,_{least,fast}}{8,16,32,64}}_t,{size,ptrdiff}_t})
    stddef_types=$(echo {ptrdiff,size,max_align,wchar}_t)
    stdint_types=$(echo {,u}int{ptr,max,{,_{least,fast}}{8,16,32,64}}_t)
    stdio_types="FILE fpos_t"
    stdlib_types=$(echo {,{,l}l}div_t)
    threads_types=$(echo {cnd,thrd{,_start},tss{,_dtor},mtx}_t once_flag)
    wchar_types=$(echo {mbstate,wint}_t tm)
    unistd_types=$(echo {ssize,gid,uid,off{,64},useconds,pid,socklen}_t)

    assert_macros=$(echo {,static_}assert NDEBUG)
    complex_macros="I"
    error_macros=$(echo E{DOM,ILSEQ,RANGE} errno)
    fenv_macros=$(echo FE_{DIVBYZERO,INEXACT,INVALID,{OVER,UNDER}FLOW,ALL_EXCEPT,DOWNWARD,TONEAREST,TOWARDZERO,UPWARD,DFL_ENV})
    inttypes_macros=$(echo PRI{d,i,o,u,x,X}{MAX,PTR,{,LEAST,FAST}{8,16,32,64}} SCN{d,i,o,u,x}{MAX,PTR,{,LEAST,FAST}{8,16,32,64}})
    iso646_macros=$(echo and{,_eq} bit{and,or} compl not{,_eq} or{,_eq}, xor{,_eq})
    limits_macros=$(echo {{,S,W}CHAR,SHRT,INT,{,L}LONG}_{MIN,MAX} {MB_LEN,U{CHAR,SHRT,INT,{,L}LONG}}_MAX CHAR_BIT)
    locale_macros=$(echo LC_{ALL,COLLATE,CTYPE,MONETARY,NUMERIC,TIME})
    math_macros=$(echo HUGE_VAL{,F,L} INFINITY NAN FP_{INFINITE,NAN,NORMAL,SUBNORMAL,ZERO,FAST_FMA{,F,L},ILOGB{0,NAN}} MATH_ERR{NO,EXCEPT} math_errhandling is{greater{,equal},less{,equal},lessgreater,unordered})
    setjmp_macros="setjmp"
    signal_macros=$(echo SIG{_{DFL,ERR,IGN},ABRT,FPE,ILL,INT,SEGV,TERM})
    stdarg_macros=$(echo va_{start,arg,end,copy})
    stdatomic_macros=$(echo ATOMIC_{BOOL,CHAR{,{16,32}_T},WCHAR_T,SHORT,INT,{,L}LONG,POINTER}_LOCK_FREE ATOMIC_{FLAG,VAR}_INIT memory_order_{relaxed,consume,acquire,release,acq_rel,seq_cst} kill_dependency)
    stdbool_macros="true false"
    stddef_macros="NULL"
    stdio_macros=$(echo _IO{FB,LB,NB}F BUFSIZ EOF {FOPEN,FILENAME,TMP}_MAX L_tmpnam SEEK_{CUR,END,SET} std{err,in,out})
    stdlib_macros=$(echo EXIT_{FAILURE,SUCCESS} {MB_CUR,RAND}_MAX)
    stdint_macros=$(echo {PTRDIFF,SIG_ATOMIC,WINT,INT{MAX,PTR,{,_{LEAST,FAST}}{8,16,32,64}}}_{MIN,MAX} UINT{MAX,PTR,{,_{LEAST,FAST}}{8,16,32,64}}_MAX {,U}INT{MAX,{8,16,32,64}}_C)
    threads_macros=$(echo mtx_{plain,recursive,timed} thrd_{timedout,success,busy,error,nomem} ONCE_FLAG_INIT TSS_DTOR_ITERATION)
    wchar_macros="WEOF"
    misc_macros="noreturn"
    unistd_macros=$(echo {R,W,X,F}_OK F_{{,U,T}LOCK,TEST})

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf '%s\n' "hook global WinSetOption filetype=c %{
        set-option window static_words $(join "${keywords} ${attributes} ${types} ${complex_types} ${fenv_types} ${inttypes_types} ${locale_types} ${math_types} ${setjmp_types} ${signal_types} ${stdarg_types} ${stdatomic_types} ${stddef_types} ${stdint_types} ${stdio_types} ${stdlib_types} ${threads_types} ${wchar_types} ${unistd_types} ${assert_macros} ${complex_macros} ${error_macros} ${fenv_macros} ${inttypes_macros} ${iso646_macros} ${limits_macros} ${locale_macros} ${math_macros} ${setjmp_macros} ${signal_macros} ${stdarg_macros} ${stdatomic_macros} ${stdbool_macros} ${stddef_macros} ${stdio_macros} ${stdlib_macros} ${stdint_macros} ${threads_macros} ${wchar_macros} ${misc_macros} ${unistd_macros} ${values}" ' ')
    }"

    # Highlight keywords
    printf %s "
        add-highlighter shared/c/code/keywords regex \b($(join "${keywords}" '|'))\b 0:keyword
        add-highlighter shared/c/code/attributes regex \b($(join "${attributes}" '|'))\b 0:attribute
        add-highlighter shared/c/code/types regex \b($(join "${types} ${complex_types} ${fenv_types} ${inttypes_types} ${locale_types} ${math_types} ${setjmp_types} ${signal_types} ${stdarg_types} ${stdatomic_types} ${stddef_types} ${stdint_types} ${stdio_types} ${stdlib_types} ${threads_types} ${wchar_types} ${unistd_types}" '|'))\b 0:type
        add-highlighter shared/c/code/values regex \b($(join "${assert_macros} ${complex_macros} ${error_macros} ${fenv_macros} ${inttypes_macros} ${iso646_macros} ${limits_macros} ${locale_macros} ${math_macros} ${setjmp_macros} ${signal_macros} ${stdarg_macros} ${stdatomic_macros} ${stdbool_macros} ${stddef_macros} ${stdio_macros} ${stdint_macros} ${threads_macros} ${wchar_macros} ${misc_macros} ${unistd_macros}" '|'))\b 0:value
    "
}

# c++ specific

# integer literals
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b[1-9]('?\d+)*(ul?l?|ll?u?)?\b(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b0b[01]('?[01]+)*(ul?l?|ll?u?)?\b(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b0('?[0-7]+)*(ul?l?|ll?u?)?\b(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b0x[\da-f]('?[\da-f]+)*(ul?l?|ll?u?)?\b(?!\.)} 0:value

# floating point literals
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b\d('?\d+)*\.([fl]\b|\B)(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b\d('?\d+)*\.?e[+-]?\d('?\d+)*[fl]?\b(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)(\b(\d('?\d+)*)|\B)\.\d('?[\d]+)*(e[+-]?\d('?\d+)*)?[fl]?\b(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b0x[\da-f]('?[\da-f]+)*\.([fl]\b|\B)(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b0x[\da-f]('?[\da-f]+)*\.?p[+-]?\d('?\d+)*)?[fl]?\b(?!\.)} 0:value
add-highlighter shared/cpp/code/ regex %{(?i)(?<!\.)\b0x([\da-f]('?[\da-f]+)*)?\.\d('?[\d]+)*(p[+-]?\d('?\d+)*)?[fl]?\b(?!\.)} 0:value

# character literals (no multi-character literals)
add-highlighter shared/cpp/code/char regex %{(\b(u8|u|U|L)|\B)'((\\.)|[^'\\])'\B} 0:value

evaluate-commands %sh{
    # Grammar
    keywords="alignas alignof and and_eq asm bitand bitor break case catch
              compl const_cast continue decltype default delete do dynamic_cast
              else explicit for goto if new not not_eq operator or or_eq
              reinterpret_cast return sizeof static_assert static_cast switch
              throw try typeid using while xor xor_eq"
    attributes="auto class const constexpr enum extern final friend inline
                mutable namespace noexcept override private protected public
                register static struct template thread_local typedef typename
                union virtual volatile"
    types="bool byte char char16_t char32_t double float int long max_align_t
           nullptr_t ptrdiff_t short signed size_t unsigned void wchar_t"
    values="NULL false nullptr this true"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=cpp %{
        set-option window static_words $(join "${keywords} ${attributes} ${types} ${values}" ' ')
    }"

    # Highlight keywords
    printf %s "
        add-highlighter shared/cpp/code/keywords regex \b($(join "${keywords}" '|'))\b 0:keyword
        add-highlighter shared/cpp/code/attributes regex \b($(join "${attributes}" '|'))\b 0:attribute
        add-highlighter shared/cpp/code/types regex \b($(join "${types}" '|'))\b 0:type
        add-highlighter shared/cpp/code/values regex \b($(join "${values}" '|'))\b 0:value
    "
}

# c and c++ compiler macros
evaluate-commands %sh{
    builtin_macros="__cplusplus|__STDC_HOSTED__|__FILE__|__LINE__|__DATE__|__TIME__|__STDCPP_DEFAULT_NEW_ALIGNMENT__"

    printf %s "
        add-highlighter shared/c/code/macros regex \b(${builtin_macros})\b 0:builtin
        add-highlighter shared/cpp/code/macros regex \b(${builtin_macros})\b 0:builtin
    "
}

# objective-c specific
add-highlighter shared/objc/code/number regex %{\b-?\d+[fdiu]?|'((\\.)?|[^'\\])'} 0:value

evaluate-commands %sh{
    # Grammar
    keywords="break case continue default do else for goto if return switch
              while"
    attributes="IBAction IBOutlet __block assign auto const copy enum extern
                inline nonatomic readonly retain static strong struct typedef
                union volatile weak"
    types="BOOL CGFloat NSInteger NSString NSUInteger bool char float
           instancetype int long short signed size_t unsigned void"
    values="FALSE NO NULL TRUE YES id nil self super"
    decorators="autoreleasepool catch class end implementation interface
                property protocol selector synchronized synthesize try"

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "hook global WinSetOption filetype=objc %{
        set-option window static_words $(join "${keywords} ${attributes} ${types} ${values} ${decorators}" ' ')
    }"

    # Highlight keywords
    printf %s "
        add-highlighter shared/objc/code/keywords regex \b($(join "${keywords}" '|'))\b 0:keyword
        add-highlighter shared/objc/code/attributes regex \b($(join "${attributes}" '|'))\b 0:attribute
        add-highlighter shared/objc/code/types regex \b($(join "${types}" '|'))\b 0:type
        add-highlighter shared/objc/code/values regex \b($(join "${values}" '|'))\b 0:value
        add-highlighter shared/objc/code/decorators regex  @($(join "${decorators}" '|'))\b 0:attribute
    "
}

hook global WinSetOption filetype=(c|cpp|objc) %[
    try %{ # we might be switching from one c-family language to another
        remove-hooks window c-family-.+
    }

    hook -group c-family-indent window ModeChange insert:.* c-family-trim-autoindent
    hook -group c-family-insert window InsertChar \n c-family-insert-on-newline
    hook -group c-family-indent window InsertChar \n c-family-indent-on-newline
    hook -group c-family-indent window InsertChar \{ c-family-indent-on-opening-curly-brace
    hook -group c-family-indent window InsertChar \} c-family-indent-on-closing-curly-brace
    hook -group c-family-insert window InsertChar \} c-family-insert-on-closing-curly-brace

    alias window alt c-family-alternative-file
]

hook global WinSetOption filetype=(?!c)(?!cpp)(?!objc).* %[
    remove-hooks window c-family-.+

    unalias window alt c-family-alternative-file
]

hook -group c-highlight global WinSetOption filetype=c %[ add-highlighter window/c ref c ]
hook -group c-highlight global WinSetOption filetype=(?!c).* %[ remove-highlighter window/c ]

hook -group cpp-highlight global WinSetOption filetype=cpp %[ add-highlighter window/cpp ref cpp ]
hook -group cpp-highlight global WinSetOption filetype=(?!cpp).* %[ remove-highlighter window/cpp ]

hook -group objc-highlight global WinSetOption filetype=objc %[ add-highlighter window/objc ref objc ]
hook -group objc-highlight global WinSetOption filetype=(?!objc).* %[ remove-highlighter window/objc ]

declare-option -docstring %{control the type of include guard to be inserted in empty headers
Can be one of the following:
 ifdef: old style ifndef/define guard
 pragma: newer type of guard using "pragma once"} \
    str c_include_guard_style "ifdef"

define-command -hidden c-family-insert-include-guards %{
    evaluate-commands %sh{
        case "${kak_opt_c_include_guard_style}" in
            ifdef)
                echo 'execute-keys ggi<c-r>%<ret><esc>ggxs\.<ret>c_<esc><space>A_INCLUDED<esc>ggxyppI#ifndef<space><esc>jI#define<space><esc>jI#endif<space>//<space><esc>O<esc>'
                ;;
            pragma)
                echo 'execute-keys ggi#pragma<space>once<esc>'
                ;;
            *);;
        esac
    }
}

hook -group c-family-insert global BufNewFile .*\.(h|hh|hpp|hxx|H) c-family-insert-include-guards

declare-option -docstring "colon separated list of path in which header files will be looked for" \
    str-list alt_dirs '.' '..'

define-command c-family-alternative-file -docstring "Jump to the alternate file (header/implementation)" %{ evaluate-commands %sh{
    file="${kak_buffile##*/}"
    file_noext="${file%.*}"
    dir=$(dirname "${kak_buffile}")

    # Set $@ to alt_dirs
    eval "set -- ${kak_opt_alt_dirs}"

    case ${file} in
        *.c|*.cc|*.cpp|*.cxx|*.C|*.inl|*.m)
            for alt_dir in "$@"; do
                for ext in h hh hpp hxx H; do
                    altname="${dir}/${alt_dir}/${file_noext}.${ext}"
                    if [ -f ${altname} ]; then
                        printf 'edit %%{%s}\n' "${altname}"
                        exit
                    fi
                done
            done
        ;;
        *.h|*.hh|*.hpp|*.hxx|*.H)
            for alt_dir in "$@"; do
                for ext in c cc cpp cxx C m; do
                    altname="${dir}/${alt_dir}/${file_noext}.${ext}"
                    if [ -f ${altname} ]; then
                        printf 'edit %%{%s}\n' "${altname}"
                        exit
                    fi
                done
            done
        ;;
        *)
            echo "echo -markup '{Error}extension not recognized'"
            exit
        ;;
    esac
    echo "echo -markup '{Error}alternative file not found'"
}}
