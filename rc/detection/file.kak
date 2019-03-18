hook global BufOpenFile .* %{ evaluate-commands %sh{
    if [ -z "${kak_opt_filetype}" ]; then
        mime=$(file -b --mime-type "${kak_buffile}")
        case "${mime}" in
            application/*+xml) filetype="xml" ;;
            image/*+xml) filetype="xml" ;; #SVG
            message/rfc822) filetype="mail" ;;
            text/x-shellscript) filetype="sh" ;;
            text/x-*) filetype="${mime#text/x-}" ;;
            text/*)   filetype="${mime#text/}" ;;
            application/*) filetype="${mime#application/}" ;;
        esac
        if [ -n "${filetype}" ]; then
            printf "set-option buffer filetype '%s'\n" "${filetype}"
        fi
    fi
} }
