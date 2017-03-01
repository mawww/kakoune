## If set to true, backups will be removed as soon as they have been restored
decl bool autorestore_purge_restored true

## Insert the content of the backup file into the current buffer, if a suitable one is found
def autorestore-restore-buffer -docstring "Restore the backup for the current file if it exists" %{
    %sh{
        buffer_basename="${kak_buffile##*/}"
        buffer_dirname=$(dirname "${kak_buffile}")

        ## Find the name of the latest backup created for the buffer that was open
        backup_path=$(ls -1t ."${kak_bufname}".kak.* 2>/dev/null | head -n 1)

        if [ -z "${backup_path}" ]; then
            exit
        fi

        printf %s\\n "
            ## Replace the content of the buffer with the content of the backup file
            exec -draft %{ %d!cat<space>${backup_path}<ret>d }

            ## If the backup file has to be removed, issue the command once
            ## the current buffer has been saved
            ## If the autorestore_purge_restored option has been unset right after the
            ## buffer was restored, do not remove the backup
            hook -group autorestore buffer BufWritePost '${kak_buffile}' %{
                nop %sh{
                    if [ \"\${kak_opt_autorestore_purge_restored}\" = true ]; then
                        ls -1 '${buffer_dirname}'/.'${buffer_basename}'.kak.* 2>/dev/null | while read -r f; do
                            rm -f \"\${f}\"
                        done
                    fi
                }
            }
        "
    }
}

## Remove all the backups that have been created for the current buffer
def autorestore-purge-backups -docstring "Remove all the backups of the current buffer" %{
    nop %sh{
        if [ ! -f "${kak_buffile}" ]; then
            exit
        fi

        buffer_basename="${kak_bufname##*/}"
        buffer_dirname=$(dirname "${kak_bufname}")

        ls -1 "${buffer_dirname}"/."${buffer_basename}".kak.* 2>/dev/null | while read -r f; do
            rm -f "${f}"
        done
    }
    echo -color Information 'Backup files removed'
}

## If for some reason, backup files need to be ignored
def autorestore-disable -docstring "Disable automatic backup recovering" %{
    remove-hooks global autorestore
}

hook -group autorestore global BufCreate .* %{ autorestore-restore-buffer }
