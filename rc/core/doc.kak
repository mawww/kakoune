decl -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

def -hidden -params 1..2 doc-open %{
    %sh{
        # fallback implementation: mktemp
        export PATH="${PATH}:${kak_runtime}/sh"

        manout=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)

        # This option is handled by the `man-db` implementation
        export MANWIDTH=${kak_window_width}

        if man "$1" > "${manout}" 2>/dev/null; then
            readonly manout_noescape=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)

            sed -e $(printf 's/.\x8//g') -e 's,\x1B\[[0-9;]*[a-zA-Z],,g' "${manout}" > "${manout_noescape}"
            mv -f "${manout_noescape}" "${manout}"

            printf %s\\n "
                edit! -scratch '*doc*'
                exec |cat<space>${manout}<ret>gg
                nop %sh{rm ${manout}}
                set buffer filetype man
                remove-hooks window man-hooks
            "

            if [ $# -gt 1 ]; then
                needle=$(printf %s\\n "$2" | sed 's,<,<lt>,g')
                printf %s\\n "try %{ exec '%<a-s><a-k>(?i)^\h+[^\n]*?\Q${needle}\E<ret>\'' } catch %{ exec <space>gg }"
            fi
        else
           printf %s\\n "echo -markup %{{Error}doc '$@' failed: see *debug* buffer for details}"
           rm ${manout}
        fi
    }
}

def -params 1..2 \
    -shell-candidates %{
        find "${kak_runtime}/doc/" -type f -name "*.gz" | while read l; do
            basename "${l%.*}"
        done
    } \
    doc -docstring %{doc <topic> [<keyword>]: open a buffer containing documentation about a given topic
An optional keyword argument can be passed to the function, which will be automatically selected in the documentation} %{
    %sh{
        readonly PATH_DOC="${kak_runtime}/doc/${1}.gz"

        shift
        if [ -f "${PATH_DOC}" ]; then
            printf %s\\n "eval -try-client %opt{docsclient} doc-open ${PATH_DOC} $@"
        else
            printf %s\\n "echo -markup '{Error}No such doc file: ${PATH_DOC}'"
        fi
    }
}

alias global help doc
