decl str docsclient

def -hidden -params 1..2 doc-open %{
    %sh{
        manout=$(mktemp /tmp/kak-man-XXXXXX)

        # Those options are handled by the `man-db` implementation
        export MANWIDTH=${kak_window_width}

        if man "$1" > "${manout}"; then
            sed -ie $(printf 's/.\x8//g') "${manout}"

            printf %s\\n "
                edit! -scratch '*doc*'
                exec |cat<space>${manout}<ret>gg
                nop %sh{rm ${manout}}
                set buffer filetype man
            "

            if [ $# -gt 1 ]; then
                needle=$(printf %s\\n "$2" | sed 's,<,<lt>,g')
                printf %s\\n "try %{ exec '%<a-s><a-k>(?i)^\h+[^\n]*?\Q${needle}\E<ret>\'' } catch %{ exec <space>gg }"
            fi
        else
           printf %s\\n "echo -color Error %{doc '$@' failed: see *debug* buffer for details}"
           rm ${manout}
        fi
    }
}

def -params 1..2 \
    -shell-candidates %{
        find "${kak_runtime}/../doc/kak/manpages/" -type f -iname "*.gz" | while read l; do
            basename "${l%.*}"
        done
    } \
    doc -docstring %{doc <topic> [<keyword>]: open a buffer containing documentation about a given topic
An optional keyword argument can be passed to the function, which will be automatically selected in the documentation} %{
    %sh{
        readonly PATH_DOC="${kak_runtime}/../doc/kak/manpages/${1}.gz"

        shift
        if [ ! -f "${PATH_DOC}" ]; then
            printf %s\\n "echo -color Error No such doc file: ${PATH_DOC}"
            exit
        fi

        printf %s\\n "eval -try-client %opt{docsclient} doc-open ${PATH_DOC} $@"
    }
}

alias global help doc
