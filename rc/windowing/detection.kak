# Attempt to detect the windowing environment we're operating in
#
# We try to load modules from the windowing_modules str-list option in order,
# stopping when one of the modules loads successfully. This ensures that only
# a single module is loaded by default.
#
# On load each module must attempt to detect the environment it's appropriate
# for, and if the environment isn't appropriate it must fail with an error.
# In addition, each module must check for the length of the windowing_modules
# str-list option defined below, and must /not/ check for an appropriate
# environment if the list is empty. An example of this test:
#
# evaluate-commands %sh{
#     [ -z "${kak_opt_windowing_modules}" ] || [ -n "$TMUX" ] || echo 'fail tmux not detected'
# }
#
# Each module is expected to define at least two aliases:
#  * terminal - create a new terminal with sensible defaults
#  * focus - focus the specified client, defaulting to the current client
#

declare-option -docstring \
"Ordered list of windowing modules to try and load. An empty list disables
both automatic module loading and environment detection, enabling complete
manual control of the module loading." \
str-list windowing_modules 'tmux' 'screen' 'zellij' 'kitty' 'iterm' 'sway' 'wayland' 'x11' 'wezterm'

declare-option -docstring %{
    windowing module to use in the 'terminal' command
} str windowing_module

declare-option -docstring %{
    where to create new windows in the 'terminal' command.

    Possible values:
    - "window" (default) - new window
    - "horizontal" - horizontal split (left/right)
    - "vertical" - vertical split (top/bottom)
    - "tab" - new tab besides current window
} str windowing_placement window

define-command terminal -params 1.. -docstring %{
    terminal <program> [<arguments>]: create a new terminal using the preferred windowing environment and placement

    This executes "%opt{windowing_module}-terminal-%opt{windowing_placement}" with the given arguments.
    If the windowing module is 'wayland', 'sway' or 'x11', then the 'termcmd' option is used as terminal program.

    Example usage:

        terminal sh
        with-option windowing_placement horizontal terminal sh

    See also the 'new' command.
} %{
    "%opt{windowing_module}-terminal-%opt{windowing_placement}" %arg{@}
}
complete-command terminal shell

# TODO Move this?
define-command with-option -params 3.. -docstring %{
    with-option <option_name> <new_value> <command> [<arguments>]: evaluate a command with a modified option
} %{
    evaluate-commands -save-regs s %{
        evaluate-commands set-register s %exp{%%opt{%arg{1}}}
        set-option current %arg{1} %arg{2}
        try %{
            evaluate-commands %sh{
                shift 2
                for arg
                do
                    printf "'%s' " "$(printf %s "$arg" | sed "s/'/''/g")"
                done
            }
        } catch %{
            set-option current %arg{1} %reg{s}
            fail "with-option: %val{error}"
        }
        set-option current %arg{1} %reg{s}
    }
}

hook -group windowing global KakBegin .* %{
    evaluate-commands %sh{
        set -- ${kak_opt_windowing_modules}
        if [ $# -gt 0 ]; then
            echo 'try %{ '
            while [ $# -ge 1 ]; do
                echo "require-module ${1}; set-option global windowing_module ${1} } catch %{ "
                shift
            done
            echo "echo -debug 'no windowing module detected' }"
        fi
    }
}
