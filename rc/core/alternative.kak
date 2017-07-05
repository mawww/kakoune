decl -docstring "colon separated list of path in which alternative files will be looked for" \
    str-list alternative_directories "."

decl -docstring "colon separated list of source file extensions" \
    str-list alternative_source_extensions
decl -docstring "colon separated list of header file extensions" \
    str-list alternative_header_extensions

hook global BufSetOption filetype=c %{
    set buffer alternative_source_extensions "c:inl"
    set buffer alternative_header_extensions "h"
}

hook global BufSetOption filetype=cpp %{
    set buffer alternative_source_extensions "cc:cpp:cxx:inl"
    set buffer alternative_header_extensions "h:hh:hpp:hxx"
}

hook global BufSetOption filetype=objc %{
    set buffer alternative_source_extensions "m"
    set buffer alternative_header_extensions "h"
}

def generic-alternative-file -docstring "Jump to the alternative file (header/implementation)" %{ %sh{
    file="${kak_buffile##*/}"
    file_noext="${file%.*}"
    file_dir=$(dirname "${kak_buffile}")

    open_alternative() {
        printf %s\\n "${1}" | (while read -r ext_source; do
            if [ ! "${file##*.}" = "${ext_source}" ]; then
                continue
            fi

            printf %s\\n "${2}" | (while read -r ext_header; do
                printf %s\\n "${3}" | (while read -r dir_alt; do
                    dir_alt=$(readlink -f "${dir_alt}")
                    file_alt="${dir_alt}/${file_noext}.${ext_header}"

                    if [ -e "${file_alt}" ]; then
                        printf 'edit %%{%s}\n' "${file_alt}"

                        exit 1
                    fi
                done) || exit $?
            done) || exit $?
        done) || return $?
    }

    dirs_alt=$(printf %s\\n "${kak_opt_alternative_directories}" | tr ':' '\n')
    exts_source=$(printf %s "${kak_opt_alternative_source_extensions}" | tr ':' '\n')
    exts_header=$(printf %s "${kak_opt_alternative_header_extensions}" | tr ':' '\n')

    cd "${file_dir}" || exit

    cmd_edit=$(open_alternative "${exts_source}" "${exts_header}" "${dirs_alt}")
    if [ -n "${cmd_edit}" ]; then
        printf %s\\n "${cmd_edit}"
    else
        cmd_edit=$(open_alternative "${exts_header}" "${exts_source}" "${dirs_alt}")
        if [ -n "${cmd_edit}" ]; then
            printf %s\\n "${cmd_edit}"
        else
            echo "echo -color Error 'alternative file not found'"
        fi
    fi
} }

alias global alternative-file generic-alternative-file
