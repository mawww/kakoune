declare-option -docstring 'root of mounted Kakoune filesystem for this session' str kakfs

define-command \
    -params 0 \
    -docstring %{mount: mount Kakoune filesystem} \
    mount %{
    hook -group kakfs global KakEnd .* unmount
    evaluate-commands %sh{
        if [ -n "$XDG_RUNTIME_DIR" ]; then
            address="unix!$XDG_RUNTIME_DIR/kakoune/$kak_session"
            root="$XDG_RUNTIME_DIR/kakfs/$kak_session"
        else
            address="unix!/tmp/kakoune/$USER/$kak_session"
            root="/tmp/kakfs/$USER/$kak_session"
        fi
        mkdir -p "$root"
        if [ $? -ne 0 ]; then
            printf 'fail %%{mount: cannot mkdir root directory %s}\n' "$root"
            exit 1
        fi
        printf 'set-option global kakfs %%{%s}\n' "$root"
        # Must be asynchronous to avoid a deadlock
        (
            errors=$(9 9pfuse "$address" "$root" 2>&1)
            if [ $? -ne 0 ]; then
                rmdir "$root"
                (
                    printf 'evaluate-commands -try-client "%s" %%{\n' "$kak_client"
                    printf '    set-option global kakfs ""\n'
                    printf '    echo -markup %%{{Error}mount: unable to mount filesystem: %s}\n' "$errors"
                    printf '}\n'
                ) |kak -p "$kak_session"
            fi
        ) </dev/null >/dev/null 2>&1 &
    }
}

define-command \
    -params 0 \
    -docstring %{unmount: unmount Kakoune filesystem} \
    unmount %{
    evaluate-commands %sh{
        if [ -z "$kak_opt_kakfs" ]; then
            exit 0
        fi
        if command -v fusermount >/dev/null; then
            umount_command='fusermount -u'
        else
            umount_command='umount'
        fi
        # Must be asynchronous to avoid a deadlock
        (
            errors=$($umount_command "$kak_opt_kakfs" 2>&1)
            if [ $? -ne 0 ]; then
                (
                    printf 'evaluate-commands -try-client "%s" %%{\n' "$kak_client"
                    printf '    echo -markup %%{{Error}unmount: %s: %s}\n' "$umount_command" "$errors"
                    printf '}\n'
                ) |kak -p "$kak_session"
                exit
            fi
            rmdir "$kak_opt_kakfs"
            printf 'set-option global kakfs ""\n' |kak -p "$kak_client"
        ) </dev/null >/dev/null 2>&1 &
    }
}
