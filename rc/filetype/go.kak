# https://golang.org/
#

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*\.go %{
    set-option buffer filetype go
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=go %{
    require-module go

    set-option window static_words %opt{go_static_words}

    # cleanup trailing whitespaces when exiting insert mode
    hook window ModeChange pop:insert:.* -group go-trim-indent %{ try %{ execute-keys -draft xs^\h+$<ret>d } }
    hook window InsertChar \n -group go-indent go-indent-on-new-line
    hook window InsertChar \{ -group go-indent go-indent-on-opening-curly-brace
    hook window InsertChar \} -group go-indent go-indent-on-closing-curly-brace
    hook window InsertChar \n -group go-comment-insert go-insert-comment-on-new-line
    hook window InsertChar \n -group go-closing-delimiter-insert go-insert-closing-delimiter-on-new-line

    alias window alt go-alternative-file

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window go-.+
        unalias window alt go-alternative-file
    }
}

hook -group go-highlight global WinSetOption filetype=go %{
    add-highlighter window/go ref go
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/go }
}

provide-module go %§

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/go regions
add-highlighter shared/go/code default-region group
add-highlighter shared/go/back_string region '`' '`' fill string
add-highlighter shared/go/double_string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/go/single_string region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/go/comment region /\* \*/ fill comment
add-highlighter shared/go/comment_line region '//' $ fill comment

add-highlighter shared/go/code/ regex %{-?([0-9]*\.(?!0[xX]))?\b([0-9]+|0[xX][0-9a-fA-F]+)\.?([eE][+-]?[0-9]+)?i?\b} 0:value

evaluate-commands %sh{
    # Grammar
    keywords='break default func interface select case defer go map struct
              chan else goto package switch const fallthrough if range type
              continue for import return var'
    types='any bool byte chan comparable complex128 complex64 error float32 float64 int int16 int32
           int64 int8 interface intptr map rune string struct uint uint16 uint32 uint64 uint8 uintptr'
    values='false true nil iota'
    functions='append cap close complex copy delete imag len make new panic print println real recover'

    join() { sep=$2; eval set -- $1; IFS="$sep"; echo "$*"; }

    # Add the language's grammar to the static completion list
    printf %s\\n "declare-option str-list go_static_words $(join "${keywords} ${attributes} ${types} ${values} ${functions}" ' ')"

    # Highlight keywords
    printf %s "
        add-highlighter shared/go/code/ regex \b($(join "${keywords}" '|'))\b 0:keyword
        add-highlighter shared/go/code/ regex \b($(join "${attributes}" '|'))\b 0:attribute
        add-highlighter shared/go/code/ regex \b($(join "${types}" '|'))\b 0:type
        add-highlighter shared/go/code/ regex \b($(join "${values}" '|'))\b 0:value
        add-highlighter shared/go/code/ regex \b($(join "${functions}" '|'))\b 0:builtin
        add-highlighter shared/go/code/ regex := 0:attribute
    "
}

# Commands
# ‾‾‾‾‾‾‾‾

define-command go-alternative-file -docstring 'Jump to the alternate file (implementation ↔ test)' %{ evaluate-commands %sh{
    case $kak_buffile in
        *_test.go)
            altfile=${kak_buffile%_test.go}.go
            test ! -f "$altfile" && echo "fail 'implementation file not found'" && exit
        ;;
        *.go)
            altfile=${kak_buffile%.go}_test.go
            test ! -f "$altfile" && echo "fail 'test file not found'" && exit
        ;;
        *)
            echo "fail 'alternative file not found'" && exit
        ;;
    esac
    printf "edit -- '%s'" "$(printf %s "$altfile" | sed "s/'/''/g")"
}}

define-command -hidden go-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        try %{
            try %{ # line comment
                execute-keys -draft kx s ^\h*// <ret>
            } catch %{ # block comment
                execute-keys -draft <a-?> /\* <ret> <a-K>\*/<ret>
            }
        } catch %{
            # indent after lines with an unclosed { or (
            try %< execute-keys -draft [c[({],[)}] <ret> <a-k> \A[({][^\n]*\n[^\n]*\n?\z <ret> j<a-gt> >
            # indent after a switch's case/default statements
            try %[ execute-keys -draft kx <a-k> ^\h*(case|default).*:$ <ret> j<a-gt> ]
            # deindent closing brace(s) when after cursor
            try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
        }
    =
~

define-command -hidden go-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden go-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

define-command -hidden go-insert-comment-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K/{2,}\h* <ret> y<c-o>P<esc> }
    ]
]

define-command -hidden go-insert-closing-delimiter-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # Wisely add '}'.
        evaluate-commands -save-regs x %[
            # Save previous line indent in register x.
            try %[ execute-keys -draft kxs^\h+<ret>"xy ] catch %[ reg x '' ]
            try %[
                # Validate previous line and that it is not closed yet.
                execute-keys -draft kx <a-k>^<c-r>x.*\{\h*\(?\h*$<ret> j}iJx <a-K>^<c-r>x\)?\h*\}<ret>
                # Insert closing '}'.
                execute-keys -draft o<c-r>x}<esc>
                # Delete trailing '}' on the line below the '{'.
                execute-keys -draft xs\}$<ret>d
            ]
        ]

        # Wisely add ')'.
        evaluate-commands -save-regs x %[
            # Save previous line indent in register x.
            try %[ execute-keys -draft kxs^\h+<ret>"xy ] catch %[ reg x '' ]
            try %[
                # Validate previous line and that it is not closed yet.
                execute-keys -draft kx <a-k>^<c-r>x.*\(\h*$<ret> J}iJx <a-K>^<c-r>x\)<ret>
                # Insert closing ')'.
                execute-keys -draft o<c-r>x)<esc>
                # Delete trailing ')' on the line below the '('.
                execute-keys -draft xs\)\h*\}?\h*$<ret>d
            ]
        ]
    ]
]

§
