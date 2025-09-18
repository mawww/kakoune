# https://yalter.github.io/niri/
# ------------------------------

# Kakoune Niri windowing module
# original author: Daniel <daniel AT ficd DOT sh>
# (contact with questions or problems)

provide-module niri %~

# ensure we're actually in niri
# and that another module isn't already loaded
evaluate-commands %sh{
    [ -z "${kak_opt_windowing_modules}" ] || 
    [ -n "$NIRI_SOCKET" ] ||
    echo 'fail NIRI_SOCKET is not set'
}

# load wayland module
require-module wayland

alias global niri-terminal-window wayland-terminal-window

# Niri is a scrolling window manager;
# this module intentionally does not implement the
# vertical|horizontal naming scheme.
# Instead, we implement three ways to open a new window:
# normal: simply spawn a new window to the right.
# consume: spawn new window and consume into current column
# consume-right: spawn new window and consume into column to the right

# first arg MUST be left|right
# rest are passed to program in new terminal
# note that left means consume into current column
# because new windows are always opened to the right
define-command -hidden niri-terminal-consume-impl -params 1.. %{
    # this block is a bit confusing but i'll do my best to explain
    # when niri creates a new window, it's always in a new column
    # to the right of the focused window, and the new window is
    # focused.
    # so the idea to spawn a terminal, and then consume it
    # either into the current col, or the col to the right.
    # however, after spawning the terminal, we can't safely
    # operate on it yet because it takes some time (aprx. 0.1s)
    # to actually initialize.
    # so here's what we do:
    # 1. open niri's event stream, wait for it to initialize
    # 2. execute our command (opening new terminal window)
    # 3. watch the event stream and wait for window to be spawned
    # 3.1 only operate on window if not floating
    # 3.2 switch focus to new window if not already focused
    # 4. once it exists, run the appropriate consume command
    nop %sh{
        # first arg always left|right
        direction=$1
        shift
        # create fifo
        temp=$(mktemp -d kak-niri-XXXXXX)
        fifo="$temp/fifo"
        mkfifo "$fifo"
        # register func to cleanup fifo
        cleanup() {
        	rm -rf "$temp"
        }
        trap cleanup EXIT INT TERM
        # read event stream to fifo
        niri msg -j event-stream >"$fifo" &
        stream_pid=$!
        # register timeout fallback
        (
        	sleep 3
        	kill "$!" 2>/dev/null
        	kill "$stream_pid" 2>/dev/null
        ) >/dev/null 2>&1 </dev/null &
        watchdog_pid=$!

        # to safely quote arguments
        kakquote() {
        	set -- "$*" ""
        	while [ "${1#*\'}" != "$1" ]; do
        		set -- "${1#*\'}" "$2${1%%\'*}''"
        	done
        	printf "'%s' " "$2$1"
        }
        # loop over args and quote 'em all
        quoted_args=''
        for arg in "$@"; do
        	quoted_args="$quoted_args$(kakquote "$arg")"
        done
        {
        	ready=false
        	# read the event stream
        	while IFS= read -r line; do
        		case "$line" in
        		# this line means stream is initialized
        		*"OverviewOpenedOrClosed"*)
        			if [ "$ready" = false ]; then
        				ready=true
        				# spawn terminal with command
        				printf 'wayland-terminal-window %s' "$quoted_args" >"$kak_command_fifo"
        			fi
        			;;
        		# window is now available
        		*"WindowOpenedOrChanged"*)
        		    # only operate if window isn't floating
        			if expr "${line}" : '.*"is_floating":[[:space:]]*false.*' \
        				>/dev/null; then
        				# handle new window unfocused
        				if expr "${line}" : '.*"is_focused":[[:space:]]*false.*' \
        					>/dev/null; then
        					# extract the window ID
        					id="$(echo "$line" | sed -n 's/.*"id":[[:space:]]*\([0-9][0-9]*\).*/\1/p')"
        					niri msg action focus-window --id "$id"
        				fi
        				# operate on the new window
        				niri msg action consume-or-expel-window-${direction}
        			fi
        			# close the stream reader
        			kill "$stream_pid"
        			wait "$stream_pid" 2>/dev/null
        			kill "$watchdog_pid" 2>/dev/null
        			exit 0
        			;;
        		esac
        	done <"$fifo"
        } >/dev/null 2>&1 </dev/null &
    }
}

define-command niri-terminal-consume -params 1.. -docstring '
    niri-terminal-consume <program> [<arguments>]: create a new terminal as a Niri window and consume it into the current column
    The program passed as argument will be executed in the new terminal' \
%{
    niri-terminal-consume-impl left %arg{@}
}
complete-command niri-terminal-consume shell

define-command niri-terminal-consume-right -params 1.. -docstring '
    niri-terminal-consume-right <program> [<arguments>]: create a new terminal as a Niri window and consume it into the column to the right
    The program passed as argument will be executed in the new terminal' \
%{
    niri-terminal-consume-impl right %arg{@}
}
complete-command niri-terminal-consume-right shell

define-command niri-new-consume -params .. -docstring '
niri-new-consume [<commands>]: create a new Kakoune client and consume it into the current column
The optional arguments are passed as commands to the new client' \
%{
    niri-terminal-consume kak -c %val{session} -e "%arg{@}"
}
complete-command -menu niri-new-consume command

define-command niri-new-consume-right -params .. -docstring '
niri-new-consume-right [<commands>]: create a new Kakoune client and consume it into the column on the right
The optional arguments are passed as commands to the new client' \
%{
    niri-terminal-consume-right kak -c %val{session} -e "%arg{@}"
}
complete-command -menu niri-new-consume-right command

define-command niri-new-window -params .. -docstring '
niri-new-window [<commands>]: create a new Kakoune client
The ''niri-terminal-window'' command is used to determine the user''s preferred terminal emulator
The optional arguments are passed as commands to the new client' \
%{
    niri-terminal-window kak -c %val{session} -e "%arg{@}"
}
complete-command -menu niri-new-window command
~
