decl str docsclient

decl -hidden str _manpage

hook -group man-highlight global WinSetOption filetype=man %{
    add-highlighter group man-highlight
    # Sections
    add-highlighter -group man-highlight regex ^\S.*?$ 0:blue
    # Subsections
    add-highlighter -group man-highlight regex '^ {3}\S.*?$' 0:default+b
    # Command line options
    add-highlighter -group man-highlight regex '^ {7}-[^\s,]+(,\s+-[^\s,]+)*' 0:yellow
    # References to other manpages
    add-highlighter -group man-highlight regex [-a-zA-Z0-9_.]+\(\d\) 0:green
}

hook global WinSetOption filetype=man %{
    hook -group man-hooks window WinResize .* %{
        man-impl %opt{_manpage}
    }
}

hook -group man-highlight global WinSetOption filetype=(?!man).* %{ remove-highlighter man-highlight }

hook global WinSetOption filetype=(?!man).* %{
    remove-hooks window man-hooks
}

def -hidden -params 1..2 man-impl %{ %sh{
    manout=$(mktemp /tmp/kak-man-XXXXXX)
    colout=$(mktemp /tmp/kak-man-XXXXXX)
    MANWIDTH=${kak_window_width} man "$@" > $manout
    retval=$?
    col -b -x > ${colout} < ${manout}
    rm ${manout}
    if [ "${retval}" -eq 0 ]; then
        printf %s\\n "
                edit -scratch '*man*'
                exec '%|cat<space>${colout}<ret>gk'
                nop %sh{rm ${colout}}
                set buffer filetype man
                set window _manpage '$@'
        "
    else
       printf %s\\n "echo -color Error %{man '$@' failed: see *debug* buffer for details }"
       rm ${colout}
    fi
} }

def -params ..1 \
  -shell-completion %{
    prefix=$(printf %s\\n "$1" | cut -c1-${kak_pos_in_token} 2>/dev/null)
    for page in /usr/share/man/*/${prefix}*.[1-8]*; do
        candidate=$(basename ${page%%.[1-8]*})
        pagenum=$(printf %s\\n "$page" | sed 's,^.*\.\([1-8].*\)\..*$,\1,')
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
    pagenum=$(expr "$subject" : '.*(\([1-8].*\))')
    if [ -n "$pagenum" ]; then
        subject=${subject%%\(*}
    fi

    printf %s\\n "eval -collapse-jumps -try-client %opt{docsclient} man-impl $pagenum $subject"
} }
