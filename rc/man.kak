decl str docsclient

hook global WinSetOption filetype=man %{
    addhl group man-highlight
    addhl -group man-highlight regex ^\S.*?$ 0:blue
    addhl -group man-highlight regex ^\h+-+[-a-zA-Z_]+ 0:yellow
    addhl -group man-highlight regex [-a-zA-Z_.]+\(\d\) 0:green
    hook window -group man-hooks NormalKey <c-m> man
    set buffer tabstop 8
}

hook global WinSetOption filetype=(?!man).* %{
    rmhl man-higlight
    rmhooks window man-hooks
}

def -hidden -params .. _man %{ %sh{
    manout=$(mktemp /tmp/kak-man-XXXXXX)
    colout=$(mktemp /tmp/kak-man-XXXXXX)
    MANWIDTH=${kak_window_width} man "$@" > $manout
    retval=$?
    col -b > ${colout} < ${manout}
    rm ${manout}
    if [ "${retval}" -eq 0 ]; then
        echo "${output}" |
        echo "edit! -scratch '*man*'
              exec |cat<space>${colout}<ret>gk
              nop %sh{rm ${colout}}
              set buffer filetype man"
    else
       echo "echo -color Error %{man '$@' failed: see *debug* buffer for details }"
       rm ${colout}
    fi
} }

def -params .. \
  -shell-completion %{
    prefix=${1:0:${kak_pos_in_token}}
    for page in /usr/share/man/*/${prefix}*.[1-8]*; do
        candidate=$(basename ${page%%.[1-8]*})
        pagenum=$(echo $page | sed -r 's,^.+/.+\.([1-8][^.]*)\..+$,\1,')
        case $candidate in
            *\*) ;;
            *) echo $candidate\($pagenum\);;
        esac
    done
  } \
  man -docstring "Manpages viewer wrapper" %{ %sh{
    subject=${@-$kak_selection}
    pagenum=""

    ## The completion suggestions display the page number, strip them if present
    if [[ $subject =~ [a-zA-Z_-]+\([^\)]+\) ]]; then
        pagenum=${subject##*\(}
        pagenum=${pagenum:0:$((${#pagenum} - 1))}
        subject=${subject%%\(*}
    fi

    echo "eval -try-client %opt{docsclient} _man $pagenum $subject"
} }
