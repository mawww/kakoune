# script summary:
# a long running shell process connects to an existing gdb session and handles input/output
# input (common gdb commands) to the gdb session is done by writing to a fifo
# output is converted into corresponding kak commands to update the presented information

declare-option -hidden str gdb_dir

# contains all known breakpoints in this format:
# id|enabled|line|file:id|enabled|line|file|:...
declare-option -hidden str-list gdb_breakpoints_info
# if execution is currently stopped, contains the location in this format:
# line|file
declare-option -hidden str-list gdb_location_info
# note that these variables may reference locations that are not in currently opened buffers

# corresponding flags generated from the previous variables
# these are only set on buffer scope
declare-option -hidden line-flags gdb_breakpoints_flags
declare-option -hidden line-flags gdb_location_flag

set-face GdbBreakpoint red,default
set-face GdbLocation blue,default

add-highlighter -group / group -passes move gdb
add-highlighter -group /gdb flag_lines GdbLocation gdb_location_flag
add-highlighter -group /gdb flag_lines GdbBreakpoint gdb_breakpoints_flags

hook global WinCreate .* %{
    add-highlighter ref -passes move gdb
}

define-command -params .. -file-completion gdb-session-new %{
    gdb-session-connect-internal
    %sh{
        # can't connect until socat has created the pty thing
        while [ ! -e "${kak_opt_gdb_dir}/pty" ]; do
            sleep 1
        done
        if [ -n "$TMUX" ]; then
            tmux split-window -h " \
                gdb $@ --init-eval-command=\"new-ui mi3 ${kak_opt_gdb_dir}/pty\""
        elif [ -n "$WINDOWID" ]; then
            setsid -w $kak_opt_termcmd " \
                gdb $@ --init-eval-command=\"new-ui mi3 ${kak_opt_gdb_dir}/pty\"" 2>/dev/null >/dev/null &
        fi
    }
}

define-command gdb-session-connect %{
    gdb-session-connect-internal
    info "Please instruct gdb to \"new-ui mi3 ${kak_opt_gdb_dir}/pty\""
}


define-command -hidden gdb-session-connect-internal %{
    gdb-session-stop
    %sh{
        tmpdir=$(mktemp --tmpdir -d gdb_kak_XXX)
        mkfifo "${tmpdir}/pipe"
        {
            tail -n +1 -f "${tmpdir}/pipe" | socat "pty,link=${tmpdir}/pty" STDIO,nonblock=1 | awk '
            function send(what) {
                cmd = "kak -p '"$kak_session"'"
                print(what) | cmd
                close(cmd)
            }
            function get(input, prefix, pattern, suffix) {
                s = match(input, prefix pattern suffix)
                return substr(input, s + length(prefix), RLENGTH - length(prefix) - length(suffix))
            }
            function breakpoint_info(breakpoint) {
                id = get(breakpoint, "number=\"", "[0-9]+", "\"")
                enabled = get(breakpoint, "enabled=\"", "[yn]", "\"")
                line = get(breakpoint, "line=\"", "[0-9]+", "\"")
                file = get(breakpoint, "fullname=\"", "[^\"]*", "\"")
                return id " " enabled " " line " \"" file "\""
            }
            function frame_info(frame) {
                file = get(frame, "fullname=\"", "[^\"]*", "\"")
                line = get(frame, "line=\"", "[0-9]+", "\"")
                return line " \"" file "\""
            }
            BEGIN { connected = 0 }
            // {
                if (!connected) {
                    connected = 1
                    print("-break-list") >> "'"$tmpdir/pipe"'"
                    print("-stack-info-frame") >> "'"$tmpdir/pipe"'"
                    close("'"$tmpdir/pipe"'")
                }
            }
            /\*running/ {
                send("gdb-clear-location")
            }
            /\*stopped/ { 
                send("gdb-handle-stopped " frame_info($0))
            }
            /\^done,frame=/ {
                send("gdb-clear-location; gdb-handle-stopped " frame_info($0))
            }
            /=breakpoint-created/ {
                send("gdb-handle-breakpoint-created " breakpoint_info($0))
            }
            /=breakpoint-modified/ {
                send("gdb-handle-breakpoint-modified " breakpoint_info($0))
            }
            /=breakpoint-deleted/ {
                id = get($0, "id=\"", "[0-9]+", "\"")
                send("gdb-handle-breakpoint-deleted " id)
            }
            /\^done,BreakpointTable=/ {
                command = "gdb-clear-breakpoints"
                breakpoints_number = split($0, breakpoints, "bkpt=")
                for (i = 2; i <= breakpoints_number; i++) {
                    command = command "; gdb-handle-breakpoint-created " breakpoint_info(breakpoints[i])
                }
                send(command)
            }
            /&"print/ {
                printing = 1
                print_value = get($0, "print ", ".*", "..\"") " == "
            }
            /~".*"/ {
                if (printing == 1) {
                    print_value = print_value get($0, "= ", ".*", "\"")
                    printing = 2
                } else if (printing) {
                    print_value = print_value "\n" get($0, "\"", ".*","\"")
                }
            }
            /\^done/ {
                if (printing) {
                    # trim trailing \n
                    print_value = substr(print_value, 0, length(print_value) - 3)
                    # QUOTE => \\QUOTE
                    gsub("'\''", "\\\\'\''", print_value)
                    # eval -client $client QUOTE info  -- \QUOTE $string \QUOTE QUOTE
                    send("eval -client '"$kak_client"' '\''info -- \\'\''" print_value "\\'\'\''")
                    printing = 0
                }
            }
            '
        } 2>/dev/null >/dev/null &
        printf "$!" > "${tmpdir}/pid"
        printf "set-option global gdb_dir %s\n" "$tmpdir"
    }
    hook -group gdb global BufOpenFile .* %{
        gdb-refresh-location-flag %val{buffile}
        gdb-refresh-breakpoints-flags %val{buffile}
    }
    hook -group gdb global KakEnd .* %{
        gdb-session-stop
    }
}

define-command gdb-session-stop %{
    try %{
        %sh{
            if [ -n "$kak_opt_gdb_dir" ]; then
                kill $(ps -o pid= --ppid $(cat "${kak_opt_gdb_dir}/pid"))
                rm "${kak_opt_gdb_dir}/pid" "${kak_opt_gdb_dir}/pipe"
                rmdir "$kak_opt_gdb_dir"
            else
                printf %s\\n raise
            fi
        }
        set-option global gdb_dir ""

        gdb-clear-location
        gdb-clear-breakpoints

        remove-hooks global gdb
    }
}

define-command gdb-jump-to-location %{
    %sh{
        if [ -n "$kak_opt_gdb_location_info" ]; then
            line="${kak_opt_gdb_location_info%%|*}"
            buffer="${kak_opt_gdb_location_info#*|}"
            printf "edit -existing \"%s\" %s\n" "$buffer" "$line"
        fi
    }
}

define-command -params 1 gdb-cmd %{
    %sh{
        if [ -n "$kak_opt_gdb_dir" ]; then
            printf %s\\n "$1" > "$kak_opt_gdb_dir"/pipe
        fi
    }
}

define-command gdb-run              %{ gdb-cmd run }
define-command gdb-start            %{ gdb-cmd start }
define-command gdb-step             %{ gdb-cmd step }
define-command gdb-next             %{ gdb-cmd next }
define-command gdb-finish           %{ gdb-cmd finish }
define-command gdb-continue         %{ gdb-cmd continue }
define-command gdb-advance          %{ gdb-cmd "advance %val{buffile}:%val{cursor_line}" }
define-command gdb-set-breakpoint   %{ gdb-cmd "break %val{buffile}:%val{cursor_line}" }
define-command gdb-clear-breakpoint %{ gdb-cmd "clear %val{buffile}:%val{cursor_line}" }
define-command gdb-toggle-breakpoint %{
    try %{
        %sh{
            printf %s\\n "$kak_opt_gdb_breakpoints_info" | tr ':' '\n' |
            while read -r current; do
                buffer="${current#*|*|*|}"
                line=$(printf %s "$current" | cut -d \| -f 3)
                if [ "$buffer" = "$kak_buffile" ] && [ "$line" = "$kak_cursor_line" ]; then
                    printf %s\\n raise
                    exit
                fi
            done
        }
        gdb-set-breakpoint
    } catch %{
        gdb-clear-breakpoint
    }
}
define-command gdb-print %{ gdb-cmd "print %val{selection}" }

# implementation details commands

define-command -hidden -params 2 gdb-handle-stopped %{
    set-option global gdb_location_info "%arg{1}|%arg{2}"
    gdb-refresh-location-flag %arg{2}
}

define-command -hidden -params 1 gdb-refresh-location-flag %{
    try %{
        %sh{
            if [ -n "$kak_opt_gdb_location_info" ]; then
                line="${kak_opt_gdb_location_info%%|*}"
                buffer="${kak_opt_gdb_location_info#*|}"
                if [ "$buffer" = "$1" ]; then
                    printf "set-option -add \"buffer=%s\" gdb_location_flag \"%s|➡\"\n" "$buffer" "$line"
                fi
            fi
        }
    }
}

define-command -hidden gdb-clear-location %{
    %sh{
        if [ -n "$kak_opt_gdb_location_info" ]; then
            buffer="${kak_opt_gdb_location_info#*|}"
            printf "unset-option \"buffer=%s\" gdb_location_flag" "$buffer"
        fi
    }
    set-option global gdb_location_info ""
}

define-command -hidden -params 4 gdb-handle-breakpoint-created %{
    set-option -add global gdb_breakpoints_info "%arg{1}|%arg{2}|%arg{3}|%arg{4}"
    gdb-refresh-breakpoints-flags %arg{4}
}

define-command -hidden -params 1 gdb-handle-breakpoint-deleted %{
    %sh{
        printf "set-option global gdb_breakpoints_info \"\"\n"
        while read -r current; do
            id="${current%%|*}"
            if [ "$id" != "$1" ]; then
                printf "set-option -add global gdb_breakpoints_info \"%s\"\n" "$current"
            else
                buffer="${current#*|*|*|}"
            fi
        done <<-EOF
			$(printf %s "$kak_opt_gdb_breakpoints_info" | tr ':' '\n')
		EOF
        printf "gdb-refresh-breakpoints-flags \"%s\"\n" "$buffer"
    }
}

define-command -hidden -params 4 gdb-handle-breakpoint-modified %{
    %sh{
        printf "set-option global gdb_breakpoints_info \"\"\n"
        printf %s\\n "$kak_opt_gdb_breakpoints_info" | tr ':' '\n' |
        while read -r current; do
            id="${current%%|*}"
            if [ "$id" != "$1" ]; then
                printf "set-option -add global gdb_breakpoints_info \"%s\"\n" "$current"
            else
                printf "set-option -add global gdb_breakpoints_info \"%s|%s|%s|%s\"\n" "$1" "$2" "$3" "$4"
            fi
        done
    }
    gdb-refresh-breakpoints-flags %arg{4}
}

define-command -hidden -params 1 gdb-refresh-breakpoints-flags %{
    # buffer may not exist, so only try
    try %{
        unset-option "buffer=%arg{1}" gdb_breakpoints_flags
        %sh{
            printf %s\\n "$kak_opt_gdb_breakpoints_info" | tr ':' '\n' |
            while read -r current; do
                buffer="${current#*|*|*|}"
                if [ "$buffer" = "$1" ]; then
                    line=$(printf %s "$current" | cut -d \| -f 3)
                    enabled=$(printf %s "$current" | cut -d \| -f 2)
                    if [ "$enabled" = "y" ]; then
                        flag="●"
                    else
                        flag="○"
                    fi
                    printf "set-option -add \"buffer=%s\" gdb_breakpoints_flags %s|%s\n" "$buffer" "$line" "$flag"
                fi
            done
       }
    }
}

define-command -hidden gdb-clear-breakpoints %{
    %sh{
        if [ -n "$kak_opt_gdb_breakpoints_info" ]; then
            printf %s\\n "$kak_opt_gdb_breakpoints_info" | tr ':' '\n' |
            while read -r current; do
                buffer="${current#*|*|*|}"
                printf "unset-option \"buffer=%s\" gdb_breakpoints_flags \n" "$buffer"
            done
        fi
    }
    set-option global gdb_breakpoints_info ""
}

