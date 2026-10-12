declare-option -docstring "shell command used for the 'format-selections' and 'format-buffer' commands" \
    str formatcmd

define-command format-buffer -docstring "Format the contents of the buffer" %{
    evaluate-commands -draft %{
        execute-keys '%'
        format-selections
    }
}

define-command format-selections -docstring "Format the selections individually" %{
    evaluate-commands %sh{
        if [ -z "${kak_opt_formatcmd}" ]; then
            echo "fail 'The option ''formatcmd'' must be set'"
        fi
    }
    evaluate-commands -draft -no-hooks -save-regs 'e|' %{
        set-register e nop
        set-register '|' %exp{
            format_in="$(mktemp "${TMPDIR:-/tmp}"/kak-formatter.XXXXXX)"
            format_out="$(mktemp "${TMPDIR:-/tmp}"/kak-formatter.XXXXXX)"

            cat > "$format_in"
            eval "$kak_opt_formatcmd" < "$format_in" > "$format_out"
            if [ $? -eq 0 ]; then
                cat "$format_out"
            else
                echo "set-register e fail formatter returned an error (exit code $?)" >"$kak_command_fifo"
                cat "$format_in"
            fi
            rm -f "$format_in" "$format_out"

            # We want the format command to be literally present in this
            # command, so that Kakoune will supply any any $kak_* variables
            # it mentions, but we don't want to evaluate the command here
            # so we'll tell the shell to exit before it gets to it.
            # Just commenting it out would not be sufficient, since it might
            # include newlines.
            exit
            %opt{formatcmd}
        }
        execute-keys '|<ret>'
        %reg{e}
    }
}

alias global format format-buffer
