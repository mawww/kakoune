## If set to true, backups will be removed as soon as they have been restored
decl bool autorestore_purge_restored true

def -hidden _autorestore-restore-buffer %{
    nop %sh{
        buffer_basename="${kak_bufname##*/}"
        buffer_dirname=$(dirname "${kak_bufname}")

        ## Find the name of the latest backup created for the buffer that was open
        latest_backup_path=$(find "${buffer_dirname}" -maxdepth 1 -type f -readable -name "\.${buffer_basename}\.kak\.*" -printf '%A@/%p\n' 2>/dev/null \
                             | sort -n -t. -k1 | sed -nr 's/^.+\///;$p')
        test ! -z "${latest_backup_path}" || exit

        ## Replace the content of the buffer with the content of the backup file
        echo "
            exec -draft %{ %d!cat<space>${latest_backup_path}<ret>d }
            echo -color Information Backup restored
        "

        ## If the backup file has to be removed, issue the command once
        ## the current buffer has been saved
        ## If the autorestore_purge_restored option has been unset right after the
        ## buffer was restored, do not remove the backup
        echo "
            hook -group autorestore global BufWritePost (.+/)?${kak_bufname} %{
                nop %sh{
                    echo \"\${kak_opt_autorestore_purge_restored}\" > /tmp/out
                    if [ \"\${kak_opt_autorestore_purge_restored,,}\" = yes \
                         -o \"\${kak_opt_autorestore_purge_restored,,}\" = true ]; then
                        rm -f '${latest_backup_path}'
                    fi
                }
            }
        "
    }
}

## Remove all the backups that have been created for the current buffer
def autorestore-purge-backups -docstring "Remove all the backups of the current buffer" %{
    nop %sh{
        buffer_basename="${kak_bufname##*/}"
        buffer_dirname=$(dirname "${kak_bufname}")

        find "${buffer_dirname}" -type f -readable -name ".${buffer_basename}.kak.*" -delete 2>/dev/null
    }
}

## If for some reason, backup files need to be ignored
def autorestore-disable -docstring "Disable automatic backup recovering" %{
    rmhooks global autorestore
}

hook -group autorestore global BufCreate .* %{ _autorestore-restore-buffer }
