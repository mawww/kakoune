declare-option -docstring "maximum amount of characters per line, after which a newline character will be inserted" \
    int autowrap_column 80
declare-option -docstring "display a column highlight at the autowrap column" \
    bool autowrap_highlight false
declare-option -docstring %{
    when enabled, paragraph formatting will reformat the whole paragraph in which characters are being inserted
    This can potentially break formatting of documents containing markup (e.g. markdown)
} bool autowrap_format_paragraph no
declare-option -docstring %{
    command to which the paragraphs to wrap will be passed
    all occurences of '%c' are replaced with `autowrap_column`
} str autowrap_fmtcmd 'fold -s -w %c'

define-command -hidden autowrap-cursor %{ evaluate-commands -save-regs '/"|^@m' %{
    try %{
        ## if the line isn't too long, do nothing
        execute-keys -draft "x<a-k>^\N{%opt{autowrap_column},}\N<ret>"

        try %{
            reg m "%val{selections_desc}"

            ## if we're adding characters past the limit, just wrap them around
            execute-keys -draft "<a-h><a-k>.{%opt{autowrap_column}}\h*[^\s]*<ret>1s(\h+)[^\h]*\z<ret>c<ret>"
        } catch %{
            ## if we're adding characters in the middle of a sentence, use
            ## the `fmtcmd` command to wrap the entire paragraph
            evaluate-commands %sh{
                if [ "${kak_opt_autowrap_format_paragraph}" = true ] \
                    && [ -n "${kak_opt_autowrap_fmtcmd}" ]; then
                    format_cmd=$(printf %s "${kak_opt_autowrap_fmtcmd}" \
                                 | sed "s/%c/${kak_opt_autowrap_column}/g")
                    printf %s "
                        evaluate-commands -draft %{
                            execute-keys '<a-]>px<a-j>|${format_cmd}<ret>'
                            try %{ execute-keys s\h+$<ret> d }
                        }
                        select '${kak_main_reg_m}'
                    "
                fi
            }
        }
    }
} }

define-command autowrap-enable -docstring "Automatically wrap the lines in which characters are inserted" %{
    hook -group autowrap window InsertChar \N autowrap-cursor
}

define-command autowrap-disable -docstring "Disable automatic line wrapping" %{
    remove-hooks window autowrap
}

define-command -hidden autowrap-set-highlight %{ evaluate-commands %sh{
    if [ "$kak_opt_autowrap_highlight" = true ] \
        && [ "$kak_opt_autowrap_column" -gt 0 ] ; then
        column="$((kak_opt_autowrap_column+1))"
        printf %s "
            add-highlighter -override window/autowrap_column column ${column} default,bright-black
        "
    else
        printf %s "try %{
            remove-highlighter window/autowrap_column
        }"
    fi
} }

hook -group autowrap-highlight global WinSetOption autowrap_column=.* %{
    autowrap-set-highlight
}
hook -group autowrap-highlight global WinSetOption autowrap_highlight=.* %{
    autowrap-set-highlight
}
