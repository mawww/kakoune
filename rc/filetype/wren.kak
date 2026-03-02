provide-module -override wren %§
    add-highlighter shared/wren regions

    add-highlighter shared/wren/line_comment region '//' '$' fill comment
    add-highlighter shared/wren/block_comment region -recurse '/\*' '/\*' '\*/' fill comment

    add-highlighter shared/wren/raw_string region '"""' '"""' fill string

    add-highlighter shared/wren/string region '"' '(?<!\\)(\\\\)*"' group
    add-highlighter shared/wren/string/ fill string
    add-highlighter shared/wren/string/ regex '\\([0"\\%abefnrtv]|x[\dA-Fa-f]{2}|u[\dA-Fa-f]{4}|U[\dA-Fa-f]{8})'0:value
    add-highlighter shared/wren/string/ regex '(?<!\\)%\(.*?\)' 0:value

    add-highlighter shared/wren/code default-region group

    add-highlighter shared/wren/code/ regex '(?i)([a-z][\w_]*)\h*(?=[\(\{])'         1:function
    add-highlighter shared/wren/code/ regex '(?i)([a-z][\w_]*)=\(.*?\)\h*(?=\{)'     1:function
    add-highlighter shared/wren/code/ regex 'class\h+(?i)([a-z][\w_]*)\h*(?=\{)'     1:type
    add-highlighter shared/wren/code/ regex 'construct\h+(?i)([a-z][\w_]*)\h*(?=\()' 1:meta
    add-highlighter shared/wren/code/ regex 'var\h+(?i)([a-z][\w_]*)'                1:variable

    add-highlighter shared/wren/code/ regex '\b_[\w_]+' 0:variable

    add-highlighter shared/wren/code/ regex '\bimport\b' 0:meta
    add-highlighter shared/wren/code/ regex '\b(true|false|null)\b' 0:value
    add-highlighter shared/wren/code/ regex '\b(as|break|class|construct|continue|else|for|foreign|if|in|return|static|super|this|var|while)\b' 0:keyword
    add-highlighter shared/wren/code/ regex '\b(Bool|Class|Fiber|Fn|List|Map|Null|Num|Object|Range|Sequence|String|System)\b' 0:+b@type
    add-highlighter shared/wren/code/ regex '(-|!|~|\*|/|%|\+|\.\.\.?|<<|>>|&{1,2}|\^|\|{1,2}|[<>]=?)|\bis\b|[!=]?=|\?|:)' 0:operator

    add-highlighter shared/wren/code/ regex 'class\h+([A-Za-z][\w_]*)\h+(is\h+[A-Za-z][\w_]*)\h*(?=\{)' 1:type 2:attribute

    add-highlighter shared/wren/code/ regex '\b(?i)-?\d+\b'               0:value
    add-highlighter shared/wren/code/ regex '\b-?0x(?i)[\da-f]+\b'        0:value
    add-highlighter shared/wren/code/ regex '\b(?i)-?\d+\.\d+\b'          0:value
    add-highlighter shared/wren/code/ regex '\b(?i)-?\d+\.\d+e[+-]?\d+\b' 0:value

    add-highlighter shared/wren/code/ regex '^\h*import\h*"(.*?)"' 1:module

    add-highlighter shared/wren/code/ regex '\bFn\.new\h*(?=[\{\(])'    0:+b@value
    add-highlighter shared/wren/code/ regex '\bFiber\.new\h*(?=[\{\(])' 0:+b@value
    add-highlighter shared/wren/code/ regex '\bFiber\.current\b'        0:+b@value
    add-highlighter shared/wren/code/ regex '\bSystem\.clock\b'         0:+b@value

    add-highlighter shared/wren/code/ regex '\bFiber\.(yield|abort|suspend)\h*(?=[\{\(])'   0:+b@function
    add-highlighter shared/wren/code/ regex '\bSystem\.((print|write)(All)?)\h*(?=[\{\(])'  0:+b@function
    add-highlighter shared/wren/code/ regex '\bSystem\.gc\h*(?=\()'                         0:+b@function

    add-highlighter shared/wren/code/ regex '\bList\.filled\h*(?=\()'    0:+b@function
    add-highlighter shared/wren/code/ regex '\b(List|Map)\.new\h*(?=\()' 0:+b@value

    add-highlighter shared/wren/code/ regex '\bNum\.fromString\h*(?=\()' 0:+b@function
    add-highlighter shared/wren/code/ regex \
        '\bNum\.(infinity|nan|pi|tau|largest|smallest|(min|max)SafeInteger)\b' 0:+b@value

    add-highlighter shared/wren/code/ regex '\bObject\.same\h*(?=\()'                 0:+b@function
    add-highlighter shared/wren/code/ regex '\bString\.from(Byte|CodePoint)\h*(?=\()' 0:+b@function

    declare-option str-list wren_static_words \
        'import' 'true' 'false' 'null' 'as' 'break' 'class' 'construct' 'continue' 'else' 'for' 'foreign' 'if' 'in' 'return' 'static' 'super' 'this' \
        'var' 'while' 'Bool' 'Class' 'Fiber' 'Fn' 'List' 'Map' 'Null' 'Num' 'Object' 'Range' 'Sequence' 'String' 'System'
§

provide-module detect-wren %{

hook global BufCreate (.*/)?.*\.wren %{ set-option buffer filetype wren }

hook -group wren-highlight global WinSetOption filetype=wren %{
    require-module wren
    add-highlighter window/wren ref wren
    hook -once -always window WinSetOption filetype=.* %{
        remove-highlighter window/wren
    }
}

hook global WinSetOption filetype=wren %{
    require-module wren

    set-option window static_words %opt{wren_static_words}

    hook window ModeChange pop:insert:.* -group wren-trim-indent %{ try %{ execute-keys -draft xs^\h+$<ret>d } }
    hook window InsertChar \n -group wren-indent wren-indent-on-new-line
    hook window InsertChar \{ -group wren-indent wren-indent-on-opening-curly-brace
    hook window InsertChar \} -group wren-indent wren-indent-on-closing-curly-brace
    hook window InsertChar \n -group wren-comment-insert wren-insert-comment-on-new-line
    hook window InsertChar \n -group wren-closing-delimiter-insert wren-insert-closing-delimiter-on-new-line
}

}

require-module detect-wren

define-command -hidden wren-indent-on-new-line %~
    evaluate-commands -draft -itersel %=
        # preserve previous line indent
        try %{ execute-keys -draft <semicolon>K<a-&> }
        # cleanup trailing white spaces on the previous line
        try %{ execute-keys -draft kx s \h+$ <ret>d }
        try %<
            try %{ # line comment
                execute-keys -draft kx s ^\h*// <ret>
            } catch %{ # block comment
                execute-keys -draft <a-?> /\* <ret> <a-K>\*/<ret>
            }
        > catch %<
            # indent after lines with an unclosed { or (
            try %< execute-keys -draft [c[({],[)}] <ret> <a-k> \A[({][^\n]*\n[^\n]*\n?\z <ret> j<a-gt> >
            # deindent closing brace(s) when after cursor
            try %[ execute-keys -draft x <a-k> ^\h*[})] <ret> gh / [})] <ret> m <a-S> 1<a-&> ]
        >
    =
~

define-command -hidden wren-indent-on-opening-curly-brace %[
    # align indent with opening paren when { is entered on a new line after the closing paren
    try %[ execute-keys -draft -itersel h<a-F>)M <a-k> \A\(.*\)\h*\n\h*\{\z <ret> s \A|.\z <ret> 1<a-&> ]
]

define-command -hidden wren-indent-on-closing-curly-brace %[
    # align to opening curly brace when alone on a line
    try %[ execute-keys -itersel -draft <a-h><a-k>^\h+\}$<ret>hms\A|.\z<ret>1<a-&> ]
]

define-command -hidden wren-insert-comment-on-new-line %[
    evaluate-commands -no-hooks -draft -itersel %[
        # copy // comments prefix and following white spaces
        try %{ execute-keys -draft <semicolon><c-s>kx s ^\h*\K/{2,}\h* <ret> y<c-o>P<esc> }
    ]
]

define-command -hidden wren-insert-closing-delimiter-on-new-line %[
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
