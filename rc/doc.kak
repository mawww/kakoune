
decl str doc_path "/usr/share/doc/kak/manpages"

def -hidden -params 1..2 _doc-open %{
    %sh{
        manout=$(mktemp /tmp/kak-man-XXXXXX)
        colout=$(mktemp /tmp/kak-man-XXXXXX)

        MANWIDTH=${kak_window_width} man -l "$1" > $manout
        retval=$?

        col -b -x > ${colout} < ${manout}
        rm ${manout}

        if [ "${retval}" -eq 0 ]; then
            echo "
                edit! -scratch '*doc*'
                exec |cat<space>${colout}<ret>gg
                nop %sh{rm ${colout}}
                set buffer filetype man
            "

            if [ $# -gt 1 ]; then
                echo "try %{ exec '%<a-s><a-k>(?i)^\h+[^\n]*?\Q${2}\E<ret>\'' } catch %{ exec <space>gg }"
            fi
        else
           echo "echo -color Error %{doc '$@' failed: see *debug* buffer for details}"
           rm ${colout}
        fi
    }
}

def -params 1..2 \
    -shell-completion %{
        find "${kak_opt_doc_path}" -type f -iname "*$@*.gz" -printf '%f\n' | while read l; do
            echo "${l%.*}"
        done
    } \
    doc -docstring "Open a buffer containing the documentation about a given subject" %{
    %sh{
        readonly PATH_DOC="${kak_opt_doc_path}/${1}.gz"

        shift
        if [ ! -f "${PATH_DOC}" ]; then
            echo "echo -color Error No such doc file: ${PATH_DOC}"
            exit
        fi

        echo "eval -try-client %opt{docsclient} _doc-open ${PATH_DOC} $@"
    }
}
