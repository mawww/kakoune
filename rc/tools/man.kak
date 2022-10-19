declare-option -docstring "name of the client in which documentation is to be displayed" \
    str docsclient

declare-option -hidden str-list manpage

hook -group man-highlight global WinSetOption filetype=man %{
    add-highlighter window/man-highlight group
    # Sections
    add-highlighter window/man-highlight/ regex ^\S.*?$ 0:title
    # Subsections
    add-highlighter window/man-highlight/ regex '^ {3}\S.*?$' 0:default+b
    # Command line options
    add-highlighter window/man-highlight/ regex '^ {7}-[^\s,]+(,\s+-[^\s,]+)*' 0:list
    # References to other manpages
    add-highlighter window/man-highlight/ regex [-a-zA-Z0-9_.]+\([a-z0-9]+\) 0:header

    map window normal <ret> :man-jump<ret>

    hook -once -always window WinSetOption filetype=.* %{
      remove-highlighter window/man-highlight
      unmap window normal <ret>
    }
}

hook global WinSetOption filetype=man %{
    hook -group man-hooks window WinResize .* %{ man-impl %opt{manpage} }
    hook -once -always window WinSetOption filetype=.* %{ remove-hooks window man-hooks }
}

define-command -hidden -params ..3 man-impl %{ evaluate-commands %sh{
    buffer_name="$1"
    if [ -z "${buffer_name}" ]; then
        exit
    fi
    shift
    manout=$(mktemp "${TMPDIR:-/tmp}"/kak-man.XXXXXX)
    manerr=$(mktemp "${TMPDIR:-/tmp}"/kak-man.XXXXXX)
    colout=$(mktemp "${TMPDIR:-/tmp}"/kak-man.XXXXXX)
    env MANWIDTH=${kak_window_range##* } man "$@" > "$manout" 2> "$manerr"
    retval=$?
    if command -v col >/dev/null; then
        col -b -x > ${colout} < ${manout}
    else
        sed 's/.//g' > ${colout} < ${manout}
    fi
    rm ${manout}

    if [ "${retval}" -eq 0 ]; then
        printf %s\\n "
                edit -scratch %{*$buffer_name ${*}*}
                execute-keys '%|cat<space>${colout}<ret>gk'
                nop %sh{ rm ${colout}; rm ${manerr} }
                set-option buffer filetype man
                set-option window manpage $buffer_name $*
        "
    else
        printf '
            fail %%{%s}
            nop %%sh{ rm "%s"; rm "%s" }
        ' "$(cat "$manerr")" "${colout}" "${manerr}"
    fi
} }

define-command -params ..1 \
    -shell-script-candidates %{
        find /usr/share/man/ $(printf %s "${MANPATH}" | sed 's/:/ /') -name '*.[1-8]*' | sed 's,^.*/\(.*\)\.\([1-8][a-zA-Z]*\).*$,\1(\2),'
    } \
    -docstring %{
        man [<page>]: manpage viewer wrapper
        If no argument is passed to the command, the selection will be used as page
        The page can be a word, or a word directly followed by a section number between parenthesis, e.g. kak(1)
    } man %{ evaluate-commands %sh{
    subject=${1-$kak_selection}

    ## The completion suggestions display the page number, strip them if present
    case "${subject}" in
        *\([1-8]*\))
            pagenum="${subject##*\(}"
            pagenum="${pagenum%\)}"
            subject="${subject%%\(*}"
            ;;
        *)
            pagenum=""
            ;;
    esac

    printf %s\\n "evaluate-commands -try-client %opt{docsclient} man-impl man $pagenum $subject"
} }



# The following section of code enables a user
# to go to next or previous man page links and to follow man page links,
# for example, apropos(1), that would normally appear in SEE ALSO sections.
# The user would position the cursor on any character of the link
# and then press <ret> to change to a buffer showing the man page.

# Regex pattern defining a man page link.
# Used for determining if a selection, which may just be a link, is a link.
declare-option -hidden regex man_link1 \
  [\w_.:-]+\(\d[a-z]*\)

# Same as above but with lookbehind and lookahead patterns.
# Used for searching for a man page link.
declare-option -hidden regex man_link2 \
  "(?:^|(?<=\W))%opt{man_link1}(?=\W)"

# Define a useful command sequence for searching a given regex
# and a given sequence of search keys.
define-command -hidden man-search -params 2 %{
    set-register / %arg[1]
    try %{
        execute-keys %arg[2]
    } catch %{
        fail "Could not find man page link"
    }
}

define-command -docstring 'Go to next man page link' \
man-link-next %{ man-search %opt[man_link2] n }

define-command -docstring 'Go to previous man page link' \
man-link-prev %{ man-search %opt[man_link2] <a-n> }

define-command -docstring 'Try to jump to a man page' \
man-jump %{
  try %{ execute-keys <a-a><a-w> s %opt[man_link1] <ret> } catch %{ fail 'Not a valid man page link' }
  try %{ man } catch %{ fail 'No man page link to follow' }
}

# Suggested keymaps for a user mode
declare-user-mode man

map global man 'g' -docstring 'Jump to a man page using selected man page link' :man-jump<ret>
map global man 'j' -docstring 'Go to next man page link'                        :man-link-next<ret>
map global man 'k' -docstring 'Go to previous man page link'                    :man-link-prev<ret>
map global man 'm' -docstring 'Look up a man page'                              :man<space>
