declare-option -docstring %{
        Remove backups once they've been restored

        See `:doc autorestore` for details.
    } \
    bool autorestore_purge_restored true

## Insert the content of the backup file into the current buffer, if a suitable one is found
define-command autorestore-restore-buffer \
    -docstring %{
        Restore the backup for the current file if it exists

        See `:doc autorestore` for details.
    } \
%{
    evaluate-commands %sh{
        buffer_basename="${kak_buffile##*/}"
        buffer_dirname=$(dirname "${kak_buffile}")

        if [ -f "${kak_buffile}" ]; then
            newer=$(find "${buffer_dirname}"/".${buffer_basename}.kak."* -newer "${kak_buffile}" -exec ls -1t {} + 2>/dev/null | head -n 1)
            older=$(find "${buffer_dirname}"/".${buffer_basename}.kak."* \! -newer "${kak_buffile}" -exec ls -1t {} + 2>/dev/null | head -n 1)
        else
            # New buffers that were never written to disk.
            newer=$(ls -1t "${buffer_dirname}"/".${buffer_basename}.kak."* 2>/dev/null | head -n 1)
            older=""
        fi

        if [ -z "${newer}" ]; then
            if [ -n "${older}" ]; then
                printf %s\\n "
                    echo -debug Old backup file(s) found: will not restore ${older} .
                "
            fi
            exit
        fi

        printf %s\\n "
            ## Replace the content of the buffer with the content of the backup file
            echo -debug Restoring file: ${newer}

            execute-keys -draft %{%d!cat<space>\"${newer}\"<ret>jd}

            ## If the backup file has to be removed, issue the command once
            ## the current buffer has been saved
            ## If the autorestore_purge_restored option has been unset right after the
            ## buffer was restored, do not remove the backup
            hook -group autorestore buffer BufWritePost '${kak_buffile}' %{
                nop %sh{
                    if [ \"\${kak_opt_autorestore_purge_restored}\" = true ];
                    then
                        rm -f \"${buffer_dirname}/.${buffer_basename}.kak.\"*
                    fi
                }
            }
        "
    }
}

## Remove all the backups that have been created for the current buffer
define-command autorestore-purge-backups \
    -docstring %{
        Remove all the backups of the current buffer

        See `:doc autorestore` for details.
    } \
%{
    evaluate-commands %sh{
        [ ! -f "${kak_buffile}" ] && exit

        buffer_basename="${kak_bufname##*/}"
        buffer_dirname=$(dirname "${kak_bufname}")

        rm -f "${buffer_dirname}/.${buffer_basename}.kak."*

        printf %s\\n "
            echo -markup {Information}Backup files removed.
            "
    }
}

## If for some reason, backup files need to be ignored
define-command autorestore-disable \
    -docstring %{
        Disable automatic backup recovering

        See `:doc autorestore` for details.
    } \
%{
    remove-hooks global autorestore
}

hook -group autorestore global BufCreate .* %{ autorestore-restore-buffer }
