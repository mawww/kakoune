# https://kdl.dev
# This supports both kdl v1 and v2.
 
hook global BufCreate .*\.kdl %{
    set-option buffer filetype kdl
}

# Initialization
# ‾‾‾‾‾‾‾‾‾‾‾‾‾‾

hook global WinSetOption filetype=kdl %{
    require-module kdl

    hook window ModeChange pop:insert:.* -group kdl-trim-indent kdl-trim-indent
    hook window InsertChar .* -group kdl-indent kdl-indent-on-char
    hook window InsertChar \n -group kdl-indent kdl-indent-on-new-line

    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window kdl-.+ }
}

hook -group kdl-highlight global WinSetOption filetype=kdl %{
    add-highlighter window/kdl ref kdl
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/kdl }
}

provide-module kdl %@

# Highlighters
# ‾‾‾‾‾‾‾‾‾‾‾‾

add-highlighter shared/kdl regions
add-highlighter shared/kdl/code default-region group

# Slashdash (/-) comments are annoying to highlight properly without a proper parser due to the fact that they can comment
# out any kdl construct.
# The below is an approximation, and there are almost certainly edge cases missed.
add-highlighter shared/kdl/slashdash_entire_node_with_child region -recurse \{ ^[\s]*/-[^\n]+\{ \} fill comment
add-highlighter shared/kdl/slashdash_child region -recurse \{ /-\s*\{ \} fill comment
add-highlighter shared/kdl/slashdash_node region ^[\s]*/-[^\n]+ $ fill comment
add-highlighter shared/kdl/slashdash_string region '/-\s*"' (?<!\\)(\\\\)*" fill comment
add-highlighter shared/kdl/slashdash_triple_string region '/-\s*"""' '(?<!\\)(?:\\\\)*"""' fill string
add-highlighter shared/kdl/slashdash_raw_string region -match-capture '/-\s*r"(#+)' '"(#+)' fill comment
add-highlighter shared/kdl/slashdash_raw_string2 region -match-capture '/-\s*"(#+)' '"(#+)' fill comment
add-highlighter shared/kdl/slashdash_builtin_value region /-\s+(true|false|null) \s fill comment
add-highlighter shared/kdl/slashdash_binary region /-\s*0b[01_]+ \s fill comment
add-highlighter shared/kdl/slashdash_octal region /-\s*0o[0-7_]+ \s fill comment
add-highlighter shared/kdl/slashdash_hex region /-\s*0x[a-fA-F0-9_]+ \s fill comment
add-highlighter shared/kdl/slashdash_decimal region /-\s*[0-9-+][0-9_]* \s fill comment
add-highlighter shared/kdl/slashdash_float region /-\s*[0-9-+][0-9_]*\.[0-9_]+ \s fill comment
add-highlighter shared/kdl/slashdash_float_exp region /-\s*[0-9-+][0-9_]*(\.[0-9_]+)?[eE][-+]?[0-9_]+ \s fill comment
add-highlighter shared/kdl/slashdash_prop_string region '/-\s*[\u000021-\u00FFFF]+="' (?<!\\)(\\\\)*" fill comment
add-highlighter shared/kdl/slashdash_prop_triple_string region '/-\s*[\u000021-\u00FFFF]+="""' '(?<!\\)(\\\\)*"""' fill comment
add-highlighter shared/kdl/slashdash_prop_raw_string region -match-capture '/-\s*[\u000021-\u00FFFF]+=r"(#*)' '"(#*)' fill comment
add-highlighter shared/kdl/slashdash_prop_raw_string2 region -match-capture '/-\s*[\u000021-\u00FFFF]+="(#*)' '"(#*)' fill comment
add-highlighter shared/kdl/slashdash_prop_other region /-\s*[\u000021-\u00FFFF]+= \s fill comment
add-highlighter shared/kdl/slashdash_arg region /-\s*[\u000021-\u00FFFF]+ \s fill comment

add-highlighter shared/kdl/raw_string region -match-capture 'r(#+)"' '"(#+)' fill string
add-highlighter shared/kdl/raw_string2 region -match-capture '(#+)"' '"(#+)' fill string

add-highlighter shared/kdl/string region '"' (?<!\\)(\\\\)*" fill string
add-highlighter shared/kdl/triple_string region '"""' '(?<!\\)(?:\\\\)*"""' fill string

add-highlighter shared/kdl/comment region -recurse /\* /\* \*/ fill comment
add-highlighter shared/kdl/line_comment region // $ fill comment


add-highlighter shared/kdl/code/node regex \b([\u000021-\u00FFFF]*)\b 0:variable # Everything not covered below is a node
add-highlighter shared/kdl/code/property regex \b([\u000021-\u00FFFF]+)(=) 0:operator 1:attribute
add-highlighter shared/kdl/code/builtin_value regex [^\w](#true|#false|#null|#inf|#-inf|#nan|true|false|null)\b 0:value
add-highlighter shared/kdl/code/binary regex \b(0b[01_]+)\b 0:value
add-highlighter shared/kdl/code/octal regex \b(0o[0-7_]+)\b 0:value
add-highlighter shared/kdl/code/hex regex \b(0x[a-fA-F0-9_]+)\b 0:value
add-highlighter shared/kdl/code/decimal regex \b([0-9-+][0-9_]*)\b 0:value
add-highlighter shared/kdl/code/float regex \b([0-9-+][0-9_]*\.[0-9_]+)\b 0:value
add-highlighter shared/kdl/code/float_exp regex \b([0-9-+][0-9_]*(\.[0-9_]+)?[eE][-+]?[0-9_]+)\b 0:value


# Commands
# ‾‾‾‾‾‾‾‾

define-command -hidden kdl-trim-indent %{
    # remove trailing white spaces
    try %{ execute-keys -draft -itersel x s \h+$ <ret> d }
}

define-command -hidden kdl-indent-on-char %<
    evaluate-commands -draft -itersel %<
        # align closer token to its opener when alone on a line
        try %< execute-keys -draft <a-h> <a-k> ^\h+[\]}]$ <ret> m <a-S> 1<a-&> >
    >
>

define-command -hidden kdl-indent-on-new-line %{ 
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

@
