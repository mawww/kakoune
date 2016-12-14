decl str mimetype

hook global BufOpen .* %{ %sh{
    if [ -z "${kak_opt_filetype}" ]; then
        mime=$(file -b --mime-type "${kak_buffile}")
        case "${mime}" in
            text/x-*) filetype="${mime#text/x-}" ;;
            text/*)   filetype="${mime#text/}" ;;
        esac
        if [ -n "${filetype}" ]; then
            printf "set buffer filetype '%s'\n" "${filetype}"
        else
            printf "set buffer mimetype '%s'\n" "${mime}"
        fi
    fi
} }
