declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

declare-option -hidden str manpage

hook -group man-highlight global WinSetOption filetype=man %{
    add-highlighter window group man-highlight
    # Sections
    add-highlighter window/man-highlight regex ^\S.*?$ 0:blue
    # Subsections
    add-highlighter window/man-highlight regex '^ {3}\S.*?$' 0:default+b
    # Command line options
    add-highlighter window/man-highlight regex '^ {7}-[^\s,]+(,\s+-[^\s,]+)*' 0:yellow
    # References to other manpages
    add-highlighter window/man-highlight regex [-a-zA-Z0-9_.]+\([a-z0-9]+\) 0:green
}

hook global WinSetOption filetype=man %{
    hook -group man-hooks window WinResize .* %{
        man-impl %opt{manpage}
    }
}

hook -group man-highlight global WinSetOption filetype=(?!man).* %{ remove-highlighter window/man-highlight }

hook global WinSetOption filetype=(?!man).* %{
    remove-hooks window man-hooks
}

define-command -hidden -params 1..2 man-impl %{ %sh{
    manout=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)
    colout=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)
    MANWIDTH=${kak_window_width} man "$@" > $manout 2>/dev/null
    retval=$?
    col -b -x > ${colout} < ${manout}
    rm ${manout}
    if [ "${retval}" -eq 0 ]; then
        printf %s\\n "
                edit -scratch '*man*'
                execute-keys '%|cat<space>${colout}<ret>gk'
                nop %sh{rm ${colout}}
                set-option buffer filetype man
                set-option window manpage '$@'
        "
    else
       printf %s\\n "echo -markup %{{Error}man '$@' failed: see *debug* buffer for details}"
       rm ${colout}
    fi
} }

define-command -params ..1 \
  -shell-completion %{
    prefix=$(printf %s\\n "$1" | cut -c1-${kak_pos_in_token} 2>/dev/null)
    for page in /usr/share/man/*/${prefix}*.[1-8]*; do
        candidate=$(basename ${page%%.[1-8]*})
        pagenum=$(printf %s\\n "$page" | sed 's,^.*\.\([1-8][^.]*\).*$,\1,')
        case $candidate in
            *\*) ;;
            *) printf %s\\n "$candidate($pagenum)";;
        esac
    done
  } \
  -docstring %{man [<page>]: manpage viewer wrapper
If no argument is passed to the command, the selection will be used as page
The page can be a word, or a word directly followed by a section number between parenthesis, e.g. kak(1)} \
    man %{ %sh{
    subject=${@-$kak_selection}

    ## The completion suggestions display the page number, strip them if present
    case "${subject}" in
        *\([1-8]*\))
            pagenum="${subject##*(}"
            pagenum="${pagenum%)}"
            subject="${subject%%(*}"
            ;;
    esac

    printf %s\\n "evaluate-commands -collapse-jumps -try-client %opt{docsclient} man-impl $pagenum $subject"
} }
