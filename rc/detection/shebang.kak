declare-option -docstring "mapping between shebang exec and actual filetype" \
    str shebang_filetypes 'bash=sh zsh=sh dash=sh python3=python'

# Add the following function to a hook on BufWritePost to automatically parse shebangs
define-command -hidden shebang-parse %{
    evaluate-commands %sh{
        if [ -z "${kak_opt_filetype}" ] || [ "plain" = "${kak_opt_filetype}" ]; then
            first_line="$(head -n+1 "${kak_buffile}")"

            if [ 0 -eq "$(expr "$first_line" : "#!")" ]; then
                return
            fi

            filetype="$(basename "$(echo "$first_line" | awk -F '(/| )' '{print $NF}')")"
            filetype="$(echo "$kak_opt_shebang_filetypes" | sed -E "s/.*${filetype}=([a-z0-9^\s]*).*/\1/")"

            printf "set-option buffer filetype '%s'\n" "${filetype}"
        fi
    }
}
