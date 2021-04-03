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
    add-highlighter window/man-highlight/ regex '(\w|\d|-|_|((-|‐)\n +))+\(\d\)' 0:header

    map window normal <ret> ': man-jump<ret>'

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
    manout=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)
    manerr=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)
    colout=$(mktemp "${TMPDIR:-/tmp}"/kak-man-XXXXXX)
    env MANWIDTH=${kak_window_range##* } man "$@" > "$manout" 2> "$manerr"
    retval=$?
    col -b -x > ${colout} < ${manout}
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
define-command man-search -params 2 %{
  set-register / %arg[1]
  execute-keys %arg[2]
}

define-command -docstring 'Go to next man page link' \
man-link-next %{ man-search %opt[man_link2] n }

define-command -docstring 'Go to previous man page link' \
man-link-prev %{ man-search %opt[man_link2] <a-n> }

# Expand backward and forward, and then try to search for a man page link
define-command man-link-here %{ evaluate-commands -save-regs / %{
  man-search %opt[man_link2] '<a-?>\b\w<ret><a-;>?\)<ret>'
}} -hidden

# Search current selection for a man page link
define-command man-link %{ evaluate-commands -save-regs / %{
  man-search %opt[man_link1] s<ret>
}} -hidden

define-command -docstring 'Try to jump to a man page' \
man-jump %{
  try %{
      # Gets us reliably to the start of a word,
      # and deletes any new-line wraps before the cursor.
      try %{
        execute-keys '<a-:><a-;>l<a-f> ;[ dhs‐<ret>d'
      }
      # Move 2 chars ahead
      execute-keys '2l'
      try %{
        # Tries to delete any new-line wraps after the cursor
        execute-keys -draft 'F(s(‐|-)\n<ret>dwd;'
        # Opens the man page
        try %{ man-link } catch %{ man-link-here } catch %{ fail 'Not a valid man page link' }
        try %{ man } catch %{ fail 'No man page link to follow' }
        # Resets the new-line wrap
        try %{ execute-keys -draft ':bp<ret>uu;<a-f> :bn<ret>' }
      } catch %{
        # Tries to open the page
        try %{ man-link } catch %{ man-link-here } catch %{ fail 'Not a valid man page link' }
        try %{ man } catch %{ fail 'No man page link to follow' }
        # Resets the modifications.
        try %{ execute-keys -draft ':bp<ret>u;<a-f> :bn<ret>' }
      }
  # If we fail, undo all the damage (does
  # put you at the start of a word though but
  # close as i can get)
  } catch %{
    execute-keys "u<a-:><a-;>;"
    # Error Message
    fail "No man page link to follow"
  }
}

define-command -hidden man-jump-raw %{

}
  
# Suggested keymaps for a user mode
declare-user-mode man-mode

define-command man-mode-map -params 3 %{
  map global man-mode %arg[1] %arg[2] -docstring %arg[3]
} -hidden

  man-mode-map 'g' ': man-jump<ret>' 'Jump to a man page using selected man page link'
man-mode-map 'j' ': try %{ man-link-next }<ret>' 'Go to next man page link'
man-mode-map 'k' ': try %{ man-link-prev }<ret>' 'Go to previous man page link'
man-mode-map 'm' ': man<space>' 'Look up a man page'
