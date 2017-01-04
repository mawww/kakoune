# Maximum amount of characters per line
decl int autowrap_column 80

# If enabled, paragraph formatting will reformat the whole paragraph in which characters are being inserted
# This can potentially break formatting of documents containing markup (e.g. markdown)
decl bool autowrap_format_paragraph no
# Command to which the paragraphs to wrap will be passed, all occurences of '%c' are replaced with `autowrap_column`
decl str autowrap_fmtcmd 'fold -s -w %c'

def -hidden autowrap-cursor %{ eval -save-regs '/"|^@m' %{
    try %{
        ## if the line isn't too long, do nothing
        exec -draft "<a-x><a-k>^[^\n]{%opt{autowrap_column},}[^\n]<ret>"

        try %{
            reg m "%val{selections_desc}"

            ## if we're adding characters past the limit, just wrap them around
            exec -draft "<a-h><a-k>.{%opt{autowrap_column}}\h*[^\s]*<ret>1s(\h+)[^\h]*\'<ret>c<ret>"
        } catch %{
            ## if we're adding characters in the middle of a sentence, use
            ## the `fmtcmd` command to wrap the entire paragraph
            %sh{
                if [ "${kak_opt_autowrap_format_paragraph}" = true ] \
                    && [ -n "${kak_opt_autowrap_fmtcmd}" ]; then
                    format_cmd=$(printf %s "${kak_opt_autowrap_fmtcmd}" \
                                 | sed "s/%c/${kak_opt_autowrap_column}/g")
                    printf %s "
                        eval -draft %{
                            exec '<a-]>p<a-x><a-j>|${format_cmd}<ret>'
                            try %{ exec s\h+$<ret> d }
                        }
                        select '${kak_reg_m}'
                    "
                fi
            }
        }
    }
} }

def autowrap-enable -docstring "Automatically wrap the lines in which characters are inserted" %{
    hook -group autowrap window InsertChar [^\n] autowrap-cursor
}

def autowrap-disable -docstring "Disable automatic line wrapping" %{
    remove-hooks window autowrap
}
