declare-option -docstring "shell command run to search for subtext in a file/directory" \
    str grepcmd 'grep -RHn'

provide-module grep %{

require-module fifo
require-module jump

define-command -params .. -docstring %{
    grep [<arguments>]: grep utility wrapper
    All optional arguments are forwarded to the grep utility
    Passing no argument will perform a literal-string grep for the current selection
} grep %{
    evaluate-commands -save-regs gs %{
        set-register g %opt{grepcmd}
        set-register s %val{selection}
        evaluate-commands -try-client %opt{toolsclient} %{
            fifo -name *grep* -script %{
                trap - INT QUIT
                grepcmd=${kak_reg_g}
                selection=${kak_reg_s}
                if [ $# -eq 0 ]; then
                    case "$grepcmd" in
                    ag\ * | git\ grep\ * | grep\ * | rg\ * | ripgrep\ * | ugrep\ * | ug\ *)
                        set -- -F -- "$selection"
                        ;;
                    ack\ *)
                        set -- -Q -- "$selection"
                        ;;
                    *)
                        set -- -- "$selection"
                        ;;
                    esac
                fi
                eval "$grepcmd \"\$@\"" 2>&1 | tr -d '\r'
            } -- %arg{@}
            set-option buffer filetype grep
            set-option buffer jump_current_line 0
        }
    }
}
complete-command grep file 

hook -group grep-highlight global WinSetOption filetype=grep %{
    add-highlighter window/grep group
    add-highlighter window/grep/ regex "^([^:\n]+):(\d+):(\d+)?" 1:cyan 2:green 3:green
    add-highlighter window/grep/ line %{%opt{jump_current_line}} default+b
    hook -once -always window WinSetOption filetype=.* %{ remove-highlighter window/grep }
}

hook global WinSetOption filetype=grep %{
    map buffer normal <ret> :jump<ret>
}

define-command -hidden grep-jump %{
    jump
}

define-command grep-next-match -docstring %{alias for "jump-next *grep*"} %{
    jump-next -matching \*grep(-.*)?\*
}

define-command grep-previous-match -docstring %{alias for "jump-previous *grep*"} %{
    jump-previous -matching \*grep(-.*)?\*
}

}

hook -once global KakBegin .* %{ require-module grep }
