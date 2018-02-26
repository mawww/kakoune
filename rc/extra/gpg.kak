# Enable kakoune to open, edit, write files encrypted
# using [GNU privacy guard](https://www.gnupg.org/)

# options {
declare-option -docstring 'Set this to true to add a signature when saving the file.' bool gpg_sign false
declare-option -docstring 'Encrypt for this user id. If not set, it is a assumed that a default was configured in ~/.gnupg/gpg.conf.' str gpg_recipient 
declare-option -docstring 'Set this to a secure deletion command e.g. shred(1).' str gpg_rmcmd 'rm -f'
# }

hook -group gpg global BufOpenFile .*\.gpg %{
    set-option buffer autoreload no
    evaluate-commands %sh{
        f="${kak_buffile%.gpg}"
        umask 066
        gpg --output "$f" --yes --decrypt "${kak_buffile}"

        if [ 0 -eq $? ]; then
            printf %s\\n "
                evaluate-commands %{
                    edit! '$f'
                    set-option buffer modelinefmt '[GPG] ${kak_opt_modelinefmt}'
                    hook -group gpg buffer BufWritePost %val{buffile} gpg-encrypt
                    hook -group gpg buffer BufClose %val{buffile} gpg-cleanup
                    echo -markup {Information} Decrypted: '${kak_buffile}'
                }
            "
        else 
            printf %s\\n "
                echo -markup {Error} Failed to decrypt ${kak_buffile}
            "    
        fi
    }
}

define-command -hidden gpg-encrypt %{ evaluate-commands %sh{
        options=""
        [ -n "${kak_opt_gpg_recipient}" ] && options="-r '${kak_opt_gpg_recipient}'"
        [ "${kak_opt_gpg_sign}" = "true" ] && options="--sign ${options}"

        cp -f "${kak_buffile}.gpg" "${kak_buffile}.gpg~" 
        eval gpg " ${options}" --yes --encrypt "${kak_buffile}"

        if [ 0 -eq $? ]; then
            printf %s\\n "
                echo -markup {Information} Encrypted: ${kak_buffile}.gpg
            "
        else
            printf %s\\n "
                echo -markup {Error} Failed to encrypt: ${kak_buffile}.gpg
            "
        fi
    }

}

define-command -hidden gpg-cleanup  %{ evaluate-commands %sh{
        if [ -f "${kak_buffile}" ] && [ -n "${kak_opt_gpg_rmcmd}" ]; then
            eval " ${kak_opt_gpg_rmcmd} '${kak_buffile}'" -- </dev/null >/dev/null 2>&1 &
        fi

        printf %s\\n "
            try 'evaluate-commands delete-buffer ${kak_buffile}.gpg'
        "
    }
}
