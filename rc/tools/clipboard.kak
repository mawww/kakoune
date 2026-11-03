declare-option -docstring %{
    The shell command to which data will be piped to copy to the system clipboard
} str clipboardcmd_copy %sh{
    for cmd in "xsel -i" "pbcopy" "wl-copy -p"; do
        readonly program="${cmd%% *}"
        if command -v "${program}" >/dev/null 2>&1; then
            printf %s "${cmd}"
            exit
        fi
    done
}

declare-option -docstring %{
    The shell command which will paste the system clipboard into the current buffer
} str clipboardcmd_paste %sh{
    for cmd in "xsel -o 2>/dev/null" "pbpaste" "wl-paste -p"; do
        readonly program="${cmd%% *}"
        if command -v "${program}" >/dev/null 2>&1; then
            printf %s "${cmd}"
            exit
        fi
    done
}

define-command -params .. -docstring %{
    clipboard-copy [<data>…]: copy the given data to the system clipboard

    If no data is passed as argument to the command, the current selection is used
    If multiple selections/data arguments are to be stored into the clipboard, they are joined with a newline character
} clipboard-copy %{ evaluate-commands %sh{
    join() {
        # NOTE: this is a POSIX way of storing a newline
        # character into $IFS without spawning a subshell
        IFS='
'
        printf %s "$*"
    }

    if [ $# -eq 0 ]; then
        eval "set -- $kak_quoted_selections"
    fi

    if ! join "$@" | eval "${kak_opt_clipboardcmd_copy}"; then
        echo fail "Unable to copy data to the system clipboard"
    fi
} }

define-command -docstring %{
    Paste the contents of the system clipboard before the selection
} clipboard-paste-before %{
    execute-keys ! <space> %opt{clipboardcmd_paste} <ret>
}

define-command -docstring %{
    Paste the contents of the system clipboard after the selection
} clipboard-paste-after %{
    execute-keys <a-!> <space> %opt{clipboardcmd_paste} <ret>
}

define-command -docstring %{
    Replace the selection with the contents of the system clipboard
} clipboard-replace %{
    evaluate-commands -save-regs c %{
        set-register c %sh{ eval "${kak_opt_clipboardcmd_paste}" }
        execute-keys \"c R
    }
}

define-command -params ..1 -docstring %{
    clipboard-enable-register [<register>]: automatically copy the contents of the given register to the clipboard, when it is modified

    If no register is passed as argument, the `c` register will be used
    The built-in copy register can be referred to as `dquote`, instead of `"`
} -shell-script-candidates %{
    echo dquote
} clipboard-enable-register %{ evaluate-commands %sh{
    reg="${1:-c}"

    # Make sure the register is valid
    printf 'nop %%reg{%s}\n' "${reg}"

    case "${reg}" in
        dquote) reg='"';;
    esac

    # NOTE: The following will break with the closing-brace register
    printf '
        try clipboard-disable-register
        hook -group clipboard-bind-register global RegisterModified %%{%s} %%{
            clipboard-copy %%reg{%s}
        }
    ' "${reg}" "${reg}"
} }

define-command -docstring %{
    Disable integration of the system clipboard with the `c` register
} clipboard-disable-register %{
    remove-hooks global clipboard-bind-register
}

# NOTE: Users who discover the command interactively might not want to
# have to think about the before/after side effects, so assume they expect
# pasting to always be performed after the selection
alias global clipboard-paste clipboard-paste-after

declare-user-mode clipboard

map global clipboard y -docstring "Copy selection to clipboard"      ": clipboard-copy<ret>"
map global clipboard p -docstring "Paste clipboard after selection"  ": clipboard-paste-after<ret>"
map global clipboard P -docstring "Paste clipboard before selection" ": clipboard-paste-before<ret>"
map global clipboard r -docstring "Replace selection with clipboard" ": clipboard-replace<ret>"
map global clipboard x -docstring "Cut selection to clipboard"       ": clipboard-copy; exec d<ret>"
