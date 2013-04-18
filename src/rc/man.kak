decl str docsclient

hook global WinSetOption filetype=man %{
    addhl group man-highlight
    addhl -group man-highlight regex ^\S.*?$ 0:blue
    addhl -group man-highlight regex ^\h+-+[-a-zA-Z_]+ 0:yellow
    addhl -group man-highlight regex [-a-zA-Z_.]+\(\d\) 0:green
    hook window -id man-hooks NormalKey <c-m> man
    setb tabstop 8
}

hook global WinSetOption filetype=(?!man).* %{
    rmhl man-higlight
    rmhooks window man-hooks
}

def -shell-params man %{ %sh{
    [[ -z "$@" ]] && set -- "$kak_selection"
    # eval in the docsclient context so that kak_window_width is the good one
    if [[ -n "$kak_opt_docsclient" && "$kak_client" != "$kak_opt_docsclient" ]]; then
        echo "eval -client $kak_opt_docsclient %{ man $@ }"
        exit
    fi

    tmpfile=$(mktemp /tmp/kak-man-XXXXXX)
    MANWIDTH=${kak_window_width} man "$@" | col -b > ${tmpfile}
    if (( ${PIPESTATUS[0]} == 0 )); then
        echo "edit! -scratch '*man*'
              exec |cat<space>${tmpfile}<ret>gk
              nop %sh{rm ${tmpfile}}
              setb filetype man"
    else
       echo "echo %{man '$@' failed: see *debug* buffer for details }"
       rm ${tmpfile}
    fi
}}
