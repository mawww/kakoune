# https://yalter.github.io/niri/
# ------------------------------

# Kakoune Niri windowing module

# Niri is a scrolling window manager;
# vertical/horizontal splits aren't really a native concept.
# A workspace consists of columns of arbitrary width, where each
# column can have one or more window in it. Windows in a column
# are stacked on top of each other. To "consume" a window into a
# column is to pull it BELOW the other windows in the column.
# For example, consider:
# +-----+ +-----+                         +-----+
# |     | |     |                         |  A  |
# |  A  | |  B  |  --consume B into A-->  +-----+
# |     | |     |                         |  B  |
# +-----+ +-----+                         +-----+

# We provide three ways to open a new window.

# normal: simply spawn a new window to the right.
#   This is the default for new windows in Niri.
# Alias: horizontal
# +-----+ +-----+
# |     | |     |
# | KAK | | NEW |
# |     | |     |
# +-----+ +-----+

# consume: the new window is consumed into the current column.
# Alias: vertical
# +-----+
# | KAK |
# +-----+
# | NEW |
# +-----+

# consume-right: spawn new window and consume into column to the right
#   Note: FOO is an already open window to the right.
# Alias: none
# +-----+ +-----+
# |     | | FOO |
# | KAK | +-----+
# |     | | NEW |
# +-----+ +-----+

# In terms of closest analogues to the vertical & horizontal split paradigm,
# we alias "vertical" to "consume" and we alias "horizontal" to "normal".
# There isn't a clean alias for "consume-right", but it's a convenient command
# that users can invoke directly.

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

# first arg MUST be left|right, rest are passed to program in new terminal
# Direction is relative to the new window, which is always opened to the right.
# Therefore, left means consume into current column.
define-command -hidden niri-terminal-consume-impl -params 1.. %{
    # When Niri creates a new window, it's always in a new column
    # to the right of the focused window, and the new window is focused.
    # Niri doesn't allow us to specify where the new window should go,
    # we can only operate on it AFTER it's been spawned.
    # The idea to spawn a terminal, and then consume it
    # either into the current col, or the col to the right.
    # However, after spawning the terminal, we can't safely
    # operate on it immediately because it takes some time (aprx. 0.1s)
    # to actually initialize.

    # 1.  Open niri's event stream, wait for it to initialize.
    #       Note: event stream is newline-delimited JSON.
    # 2.  Execute command to open new terminal window.
    # 3.  Watch the event stream and wait for window to be spawned.
    # 3.a Only operate on window if not floating.
    # 3.b Switch focus to new window if not already focused.
    #     ^ (Not certain this is necessary, but better to be safe).
    # 4.  Once the window exists, consume in specified direction.
    nop %sh{
        # First arg always left|right. No need for guards b/c this is private function.
        direction=$1
        shift
        # create fifo for collecting event stream
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

        # safely quote arguments for passing to new terminal
        kakquote() {
        	set -- "$*" ""
        	while [ "${1#*\'}" != "$1" ]; do
        		set -- "${1#*\'}" "$2${1%%\'*}''"
        	done
        	printf "'%s' " "$2$1"
        }
        # loop over args and quote all
        quoted_args=''
        for arg in "$@"; do
        	quoted_args="$quoted_args$(kakquote "$arg")"
        done
        # run this block async to avoid user-facing lag
        {
            # track when new window is ready
        	ready=false
        	# read lines of event stream from fifo
        	#   Note: each line is one event.
        	#   We wait for the event that indicates stream is initialized,
        	#   then we spawn the new terminal window. The next WindowOpenedOrChanged
        	#   event immediately after running niri-terminal-window SHOULD be
        	#   the event for this window, but it's not guaranteed. There is a very,
        	#   very tiny window for a race condition here. Unfortunately, I don't
        	#   believe we have a good way to handle it because there's no way to
        	#   know the window's ID before it's spawned. This may be a good job for
        	#   the Ostrich algorithm (https://en.wikipedia.org/wiki/Ostrich_algorithm)
        	while IFS= read -r line; do
        	    # each line is a JSON object
        		case "$line" in
        		# this line means stream is initialized
        		*"OverviewOpenedOrClosed"*)
        		    # guard to avoid spawning terminal twice
        			if [ "$ready" = false ]; then
        				ready=true
        				# spawn terminal with command
        				printf 'niri-terminal-window %s' "$quoted_args" >"$kak_command_fifo"
        			fi
        			;;
        		# newly opened window is now available to act on
        		*"WindowOpenedOrChanged"*)
        		    # only operate if window isn't floating
        		    # This accursed incantation is to access the JSON field
        		    # without depending on jq.
        			if expr "${line}" : '.*"is_floating":[[:space:]]*false.*' \
        				>/dev/null; then
        				# handle new window being unfocused
        				if expr "${line}" : '.*"is_focused":[[:space:]]*false.*' \
        					>/dev/null; then
        					# extract the window ID
        					# (POSIX compliant JSON parsing is ugly, sorry)
        					id="$(echo "$line" | sed -n 's/.*"id":[[:space:]]*\([0-9][0-9]*\).*/\1/p')"
        					# focus the new window
        					niri msg action focus-window --id "$id"
        				fi
        				# consume the new window in specified direction
        				niri msg action consume-or-expel-window-${direction}
        			fi
        			# close the stream reader and timeout watchdog
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

alias global niri-terminal-vertical niri-terminal-consume
alias global niri-terminal-horizontal niri-terminal-window

~
