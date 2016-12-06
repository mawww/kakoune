decl str mimetype

hook global BufOpen .* %{ %sh{
    if [ -z "${kak_opt_filetype}" ]; then
        mime=$(file -b --mime-type "${kak_buffile}")
        printf %s\\n "${mime}" | grep -q '^text/x-'
        if [ $? -eq 0 ]; then
            printf "set buffer filetype '%s'\n" "${mime:7}"
        else
            printf "set buffer mimetype '%s'\n" "${mime}"
        fi
    fi
} }
