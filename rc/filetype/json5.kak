# https://json5.org
# -----------------

# Detection
# ---------

hook global BufCreate .*[.](json5) %{
    set-option buffer filetype json5
}

# Initialization
# --------------

hook global WinSetOption filetype=json5 %{
    require-module json
    require-module json5
        hook window ModeChange pop:insert:.* -group json-trim-indent json-trim-indent
    hook window InsertChar .* -group json5-indent json-indent-on-char
    hook window InsertChar \n -group json5-indent json5-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{
        remove-hooks window json5-.+
        remove-hooks window json-.+
    }
}

hook -group json5-highlight global WinSetOption filetype=json5 %{
    add-highlighter window/json5 ref json5
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/json5 }
}

provide-module json5 %(
    require-module json

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/json5 regions
add-highlighter shared/json5/code default-region group
add-highlighter shared/json5/string region '"' (?<!\\)(\\\\)*" group
add-highlighter shared/json5/string/default fill string
add-highlighter shared/json5/string/escape regex (\\[\\'"bfnrtv0\n])|(\\x[0-9a-f]{2})|(\\u[0-9a-f]{4})|(\\u[0-9a-f]{0,3}([^0-9a-f]))|(\\x[0-9a-f]{0,1}([^0-9a-f]))|(\\.)|(\n) 1:keyword 2:value 3:value 4:value 5:Error 6:value 7:Error 8:Error 9:Error
add-highlighter shared/json5/sq_string region "'" (?<!\\)(\\\\)*' group
add-highlighter shared/json5/sq_string/default fill string
add-highlighter shared/json5/sq_string/escape ref json5/string/escape

add-highlighter shared/json5/line_comment region // $ group
add-highlighter shared/json5/line_comment/comment fill comment
add-highlighter shared/json5/line_comment/todo regex (TODO|NOTE|FIXME): 1:meta

add-highlighter shared/json5/multiline_comment region '/\*' '\*/' fill comment

add-highlighter shared/json5/code/ regex \b(true|false|null|NaN|Infinity|\d+(?:\.\d+)?(?:[eE][+-]?\d*)?)\b 0:value



# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden json5-indent-on-new-line %<
    evaluate-commands -draft -itersel -save-regs '/"' %<
        try %{
            try %[ # line comment
                evaluate-commands -draft  %[
                    # copy the commenting prefix
                    execute-keys -save-regs '' k x s ^\h*//[!/]{0,2}\h* <ret> y
                    try %[
                        # if the previous comment isn't empty, create a new one
                        execute-keys x<a-K>^\h*//[!/]{0,2}$<ret> jxs^\h*<ret>P
                    ] catch %[
                        # TODO figure out a way to not delete empty comment in current line
                        # if there is no space and text in the previous comment, remove it completely
                        execute-keys s //.*<ret> d
                    ]
                ]
            ] catch %[ # block comment
                # if the previous line isn't within a comment scope, break
                execute-keys -draft kx <a-k>^(\h*/\*|\h+\*(?!/))<ret>

                # find comment opening, validate it was not closed, and check its using star prefixes
                execute-keys -draft <a-?>/\*<ret><a-H> <a-K>\*/<ret> <a-k>\A\h*/\*([^\n]*\n\h*\*)*[^\n]*\n\h*.\z<ret>


                try %[
                    # if the previous line is opening the comment, insert star preceeded by space
                    execute-keys -draft kx<a-k>^\h*/\*<ret>
                    execute-keys -draft i *<space><esc>
                ] catch %[
                    try %[
                        # if the next line is a comment line insert a star
                        execute-keys -draft jx<a-k>^\h+\*<ret>
                        execute-keys -draft i*<space><esc>
                    ] catch %[
                        try %[
                            # if the previous line is an empty comment line, close the comment scope
                            execute-keys -draft kx<a-k>^\h+\*\h+$<ret> x1s\*(\h*)<ret>c/<esc>
                        ] catch %[
                            # if the previous line is a non-empty comment line, add a star
                            execute-keys -draft i*<space><esc>
                        ]
                    ]
                ]

                # Copy indentation from previous line
                execute-keys -draft -save-regs '' k x s ^\h*<ret> y
                # Add the indentation copied from the previous line
                execute-keys -draft ghPgl
                # trim trailing whitespace on the previous line
                try %[ execute-keys -draft s\h+$<ret> d ]
            ]
        } catch %`
            # preserve previous line indent
            try %{ execute-keys -draft <semicolon> K <a-&> }
            # filter previous line
            try %{ execute-keys -draft k : json-trim-indent <ret> }
            # indent after lines ending with opener token
            try %< execute-keys -draft k x <a-k> [[{]\h*$ <ret> j <a-gt> >
            # deindent closer token(s) when after cursor
            try %< execute-keys -draft x <a-k> ^\h*[}\]] <ret> gh / [}\]] <ret> m <a-S> 1<a-&> >
        `
    >
>


)

