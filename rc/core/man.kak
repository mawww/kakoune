decl str docsclient

decl -hidden str _man_page
decl -hidden str _man_subject

hook global WinSetOption filetype=man %{
    addhl group man-highlight
    # Sections
    addhl -group man-highlight regex ^\S.*?$ 0:blue
    # Subsections
    addhl -group man-highlight regex '^ {3}\S.*?$' 0:default+b
    # Command line options
    addhl -group man-highlight regex '^ {7}-[^\s,]+(,\s+-[^\s,]+)*' 0:yellow
    # References to other manpages
    addhl -group man-highlight regex [-a-zA-Z0-9_.]+\(\d\) 0:green

    hook -group man-hooks window WinResize .* %{
        man "%opt{_man_subject}(%opt{_man_page})"
    }
}

hook global WinSetOption filetype=(?!man).* %{
    rmhl man-higlight
    rmhooks window man-hooks
}

def -hidden -params 1..2 _man %{ %sh{
    manout=$(mktemp /tmp/kak-man-XXXXXX)
    colout=$(mktemp /tmp/kak-man-XXXXXX)
    MANWIDTH=${kak_window_width} man "$@" > $manout
    retval=$?
    col -b -x > ${colout} < ${manout}
    rm ${manout}
    if [ "${retval}" -eq 0 ]; then
        printf %s\\n "
                edit! -scratch '*man*'
                exec |cat<space>${colout}<ret>gk
                nop %sh{rm ${colout}}
                set buffer filetype man
                eval -draft -save-regs '/sp' %{
                    exec %{
                        t( \"sy
                        /[^(]<ret> t) \"py
                    }
                    set buffer _man_subject %reg{s}
                    set buffer _man_page %reg{p}
                }
        "
    else
       printf %s\\n "echo -color Error %{man '$@' failed: see *debug* buffer for details }"
       rm ${colout}
    fi
} }

def -params .. \
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
  man -docstring "Manpages viewer wrapper" %{ %sh{
    subject=${@-$kak_selection}

    ## The completion suggestions display the page number, strip them if present
    pagenum=$(expr "$subject" : '.*(\([1-8].*\))')
    if [ -n "$pagenum" ]; then
        subject=${subject%%\(*}
    fi

    printf %s\\n "eval -collapse-jumps -try-client %opt{docsclient} _man $pagenum $subject"
} }
