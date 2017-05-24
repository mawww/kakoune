decl -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

decl -hidden str manpage

hook -group man-highlight global WinSetOption filetype=man %{
    add-highlighter group man-highlight
    # Sections
    add-highlighter -group man-highlight regex ^\S.*?$ 0:blue
    # Subsections
    add-highlighter -group man-highlight regex '^ {3}\S.*?$' 0:default+b
    # Command line options
    add-highlighter -group man-highlight regex '^ {7}-[^\s,]+(,\s+-[^\s,]+)*' 0:yellow
    # References to other manpages
    add-highlighter -group man-highlight regex [-a-zA-Z0-9_.]+\([a-z0-9]+\) 0:green
}

hook global WinSetOption filetype=man %{
    hook -group man-hooks window WinResize .* %{
        man-impl "" %opt{manpage}
    }
}

hook -group man-highlight global WinSetOption filetype=(?!man).* %{ remove-highlighter man-highlight }

hook global WinSetOption filetype=(?!man).* %{
    remove-hooks window man-hooks
}

def -hidden -params 1..3 man-impl %{ %sh{
    keyword="${1}"
    shift

    manout=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)

    export MANWIDTH=${kak_window_width}
    if man $* > "${manout}"; then
        sed -i "" -e $(printf 's/.\x8//g') -e 's,\x1B\[[0-9;]*[a-zA-Z],,g' "${manout}"

        printf '
            edit -scratch *man*
            exec -no-hooks "\%%|cat<space>%s<ret> gg"
            nop %%sh{rm -f "%s"}
            set buffer filetype man
            set window manpage "%s"
        ' "${manout}" "${manout}" "${*}"

        if [ -n "${keyword}" ]; then
            printf 'exec -no-hooks "\%%s\Q%s<ret>gi<a-l>%s"\n' "${keyword}" "'"
        fi
    else
       printf 'echo -color Error %%{man "%s" failed: see *debug* buffer for details }\n' "$@"
       rm -f "${manout}"
    fi
} }

def -params 1..2 \
  -shell-completion %{
    prefix=$(printf %s\\n "$1" | cut -c1-${kak_pos_in_token} 2>/dev/null)
    for page in /usr/share/man/*/${prefix}*.[1-8]*; do
        printf %s\\n "${page}" | sed -e 's/\(\.gz\)*$/)/' -e 's/^.*\///g' -e 's/\.\([^.]*\)$/(\1/'
    done
  } \
  -docstring %{man <page> [<keyword>]: manpage viewer wrapper
An optional keyword argument can be passed to the command, which will be automatically selected in the documentation
The page can be a word, or a word directly followed by a section number between parenthesis, e.g. kak(1)} \
    man %{ %sh{
    subject="${1}"
    keyword="${2}"

    ## The completion suggestions display the page number, strip them if present
    pagenum=$(expr "${subject}" : '.*(\([1-8].*\))')
    if [ -n "${pagenum}" ]; then
        subject=${subject%%\(*}
    fi

    printf 'eval -collapse-jumps -try-client %%opt{docsclient} %%{
        man-impl "%s" "%s" "%s"
    }\n' "${keyword}" "${pagenum}" "${subject}"
} }
