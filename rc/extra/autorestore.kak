## If set to true, backups will be removed as soon as they have been restored
decl bool autorestore_purge_restored true

## Insert the content of the backup file into the current buffer, if a suitable one is found
def autorestore-restore-buffer -docstring "Restore the backup for the current file if it exists" %{
    %sh{
        test ! -f "${kak_buffile}" && exit

        buffer_basename=$(basename "${kak_buffile}")
        buffer_dirname=$(dirname "${kak_buffile}")

        ## Find the name of the latest backup created for the buffer that was open
        ## The backup file has to have been last modified more recently than the file we are editing
        ## Other backups are removed
        backup_path=$(find "${buffer_dirname}" -maxdepth 1 -type f -name "\.${buffer_basename}\.kak\.*" \
                           \( \( -newer "${kak_buffile}" -exec ls -1t {} + \) -o \( -exec rm {} \; \) \) 2>/dev/null)

        if [ -z "${backup_path}" ]; then
            exit
        fi

        # Only get the most recent one
        backup_path=$(ls -t $backup_path | head -n1)

        printf %s\\n "
            ## Replace the content of the buffer with the content of the backup file
            exec -draft %{ %d!cat<space>${backup_path}<ret>d }
            eval -client %val{client} echo -color Information 'Backup restored'

            ## If the backup file has to be removed, issue the command once
            ## the current buffer has been saved
            ## If the autorestore_purge_restored option has been unset right after the
            ## buffer was restored, do not remove the backup
            hook -group autorestore buffer BufWritePost '${kak_buffile}' %{
                nop %sh{
                    if [ \"\${kak_opt_autorestore_purge_restored}\" = true ]; then
                        rm -f '${backup_path}'
                    fi
                }
            }
        "
    }
}

## Remove all the backups that have been created for the current buffer
def autorestore-purge-backups -docstring "Remove all the backups of the current buffer" %{
    nop %sh{
        test ! -f "${kak_buffile}" && exit

        buffer_basename=$(basename "${kak_bufname}")
        buffer_dirname=$(dirname "${kak_bufname}")

        find "${buffer_dirname}" -maxdepth 1 -type f -readable -name "\.${buffer_basename}\.kak\.*" -delete 2>/dev/null
    }
    echo -color Information 'Backup files removed'
}

## If for some reason, backup files need to be ignored
def autorestore-disable -docstring "Disable automatic backup recovering" %{
    remove-hooks global autorestore
}

hook -group autorestore global BufOpen .* %{ autorestore-restore-buffer }
