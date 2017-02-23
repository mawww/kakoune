#!/bin/sh

for compiler in "$@"; do
    cd /home/build/

    sudo mount -t proc /proc "${compiler}/proc"

    for debug_mode in yes no; do
        printf 'Compiling with compiler=%s, debug=%s\n' "${compiler}" "${debug_mode}"

        if ! sudo chroot "${compiler}" /root/kakoune/run.sh "${debug_mode}"; then
            echo "Build failed"
        else
            echo "Build passed"
        fi

        echo
    done

    sudo umount "${compiler}/proc"
done
