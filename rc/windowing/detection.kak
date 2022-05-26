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
str-list windowing_modules 'tmux' 'screen' 'kitty' 'iterm' 'sway' 'wayland' 'x11'

hook -group windowing global KakBegin .* %{

    evaluate-commands %sh{
        set -- ${kak_opt_windowing_modules}
        if [ $# -gt 0 ]; then
            echo 'try %{ '
            while [ $# -gt 1 ]; do
                echo "require-module ${1} } catch %{ "
                shift
            done
            echo "require-module ${1} }"
        fi
    }
}
