# http://json5.org
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾

# Detection
# ‾‾‾‾‾‾‾‾‾

hook global BufCreate .*[.](json5) %{
    set-option buffer filetype json5
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=json5 %{
    require-module json5

    hook window ModeChange pop:insert:.* -group json5-trim-indent json5-trim-indent
    hook window InsertChar \n -group json5-indent json5-indent-on-new-line
    hook window InsertChar .* -group json5-indent json5-indent-on-char

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window json5-.+ }
}

hook -group json5-highlight global WinSetOption filetype=json5 %{
    add-highlighter window/json5 ref json5
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/json5 }
}


provide-module json5 %(

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/json5 regions
add-highlighter shared/json5/code default-region group
add-highlighter shared/json5/string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/json5/string2 region "'" (?<!\\)(\\\\)*' fill string
add-highlighter shared/json5/comment region '//' '$' fill comment
add-highlighter shared/json5/multiline_comment region /\* \*/ fill comment

add-highlighter shared/json5/code/ regex ^(\s*[^,:\[\]\{\}\s]+): 1:variable
add-highlighter shared/json5/code/ regex ,(\s*[^,:\[\]\{\}\s]+): 1:variable
add-highlighter shared/json5/code/ regex \{(\s*[^,:\[\]\{\}\s]+): 1:variable

add-highlighter shared/json5/code/ regex \b(true|false|null|\d+(?:\.\d+)?(?:[eE][+-]?\d*)?|0x[\da-fA-F]+)\b 0:value

# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden json5-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden json5-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m <a-S> 1<a-&> >
    >
>

define-command -hidden json5-indent-on-new-line %{
    evaluate-commands -itersel -draft %{
        execute-keys <semicolon>
        try %{
            evaluate-commands -draft -save-regs '/"' %{
                # copy the commenting prefix
                execute-keys -save-regs '' k x1s^(\h*//+\h*)<ret> y
                try %{
                    # if the previous comment isn't empty, create a new one
                    execute-keys x<a-K>^\h*//+\h*$<ret> jxs^\h*<ret>P
                } catch %{
                    # if there is no text in the previous comment, remove it completely
                    execute-keys d
                }
            }

            # trim trailing whitespace on the previous line
            try %{ execute-keys -draft k x s\h+$<ret> d }
        }

        try %{
            # if the previous line isn't within a comment scope, break
            execute-keys -draft kx <a-k>^(\h*/\*|\h+\*(?!/))<ret>

            # find comment opening, validate it was not closed, and check its using star prefixes
            execute-keys -draft <a-?>/\*<ret><a-H> <a-K>\*/<ret> <a-k>\A\h*/\*([^\n]*\n\h*\*)*[^\n]*\n\h*.\z<ret>

            try %{
                # if the previous line is opening the comment, insert star preceeded by space
                execute-keys -draft kx<a-k>^\h*/\*<ret>
                execute-keys -draft i*<space><esc>
            } catch %{
               try %{
                    # if the next line is a comment line insert a star
                    execute-keys -draft jx<a-k>^\h+\*<ret>
                    execute-keys -draft i*<space><esc>
                } catch %{
                    try %{
                        # if the previous line is an empty comment line, close the comment scope
                        execute-keys -draft kx<a-k>^\h+\*\h+$<ret> x1s\*(\h*)<ret>c/<esc>
                    } catch %{
                        # if the previous line is a non-empty comment line, add a star
                        execute-keys -draft i*<space><esc>
                    }
                }
            }

            # trim trailing whitespace on the previous line
            try %{ execute-keys -draft k x s\h+$<ret> d }
            # align the new star with the previous one
            execute-keys Kx1s^[^*]*(\*)<ret>&
        }
    }
}

)
