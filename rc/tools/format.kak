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
    evaluate-commands -draft -no-hooks -save-regs '|' %{
        set-register '|' %{
            format_in="$(mktemp "${TMPDIR:-/tmp}"/kak-formatter-XXXXXX)"
            format_out="$(mktemp "${TMPDIR:-/tmp}"/kak-formatter-XXXXXX)"

            cat > "$format_in"
            eval "$kak_opt_formatcmd" < "$format_in" > "$format_out"
            if [ $? -eq 0 ]; then
                cat "$format_out"
            else
                printf 'eval -client %s %%{ fail formatter returned an error %s }\n' "$kak_client" "$?" | kak -p "$kak_session"
                cat "$format_in"
            fi
            rm -f "$format_in" "$format_out"
        }
        execute-keys '|<ret>'
    }
}

alias global format format-buffer
