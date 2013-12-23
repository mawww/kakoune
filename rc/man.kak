decl str docsclient

hook global WinSetOption filetype=man %{
    addhl group man-highlight
    addhl -group man-highlight regex ^\S.*?$ 0:blue
    addhl -group man-highlight regex ^\h+-+[-a-zA-Z_]+ 0:yellow
    addhl -group man-highlight regex [-a-zA-Z_.]+\(\d\) 0:green
    hook window -id man-hooks NormalKey <c-m> man
    set buffer tabstop 8
}

hook global WinSetOption filetype=(?!man).* %{
    rmhl man-higlight
    rmhooks window man-hooks
}

def -hidden -shell-params _man %{ %sh{
    tmpfile=$(mktemp /tmp/kak-man-XXXXXX)
    MANWIDTH=${kak_window_width} man "$@" | col -b > ${tmpfile}
    if (( ${PIPESTATUS[0]} == 0 )); then
        echo "edit! -scratch '*man*'
              exec |cat<space>${tmpfile}<ret>gk
              nop %sh{rm ${tmpfile}}
              set buffer filetype man"
    else
       echo "echo %{man '$@' failed: see *debug* buffer for details }"
       rm ${tmpfile}
    fi
} }

def -shell-params man %{ %sh{
    [[ -z "$@" ]] && set -- "$kak_selection"
    echo "eval -try-client %opt{docsclient} _man $@"
} }
